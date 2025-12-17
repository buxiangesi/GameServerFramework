#include <cstdio>
#include <unistd.h>
#include <functional>
#include <memory>
#include <utility>
#include <string>        // ← 新增：std::string
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/socket.h>  // ← 新增
#include <cstring>       // ← 新增
#include <fcntl.h>       // ← 新增
#include <sys/stat.h>    // ← 新增
#include "Epoll.h"
#include "Thread.h"      // ← 新增：线程封装
#include "CThreadPool.h" // ← 新增：线程池
#include"Logger.h"
#include <iostream>
class CProcess
{
public:
    CProcess() = default;

    template<typename Func, typename... Args>
    void SetEntryFunction(Func&& func, Args&&... args)
    {
        m_func = [func = std::forward<Func>(func),
            ... args = std::forward<Args>(args)]() mutable -> int {
            return func(std::forward<Args>(args)...);
            };
    }

    int CreateSubProcess()
    {
        if (!m_func) {
            return -1;
        }

        int ret = socketpair(AF_LOCAL, SOCK_STREAM, 0, m_ipc_sockets);
        if (ret == -1) {
            perror("socketpair");
            return -2;
        }
        /*  ✅ 正确顺序（先创建再fork）：
          T1: socketpair() → m_ipc_sockets[0]=3, m_ipc_sockets[1]=4
          T2: fork() → 子进程继承 fd=3 和 fd=4
          结果：父子进程都有这两个socket

          ❌ 错误顺序（先fork再创建）：
          T1: fork()
          T2: 父进程 socketpair() → m_ipc_sockets[0]=3, m_ipc_sockets[1]=4
          结果：只有父进程有socket，子进程没有！*/


        pid_t pid = fork();
        if (pid == -1) {
            return -3;
        }

        if (pid == 0) {
            // 子进程
            close(m_ipc_sockets[1]);      // ← 新增：关闭写端
            m_ipc_sockets[1] = 0;         // ← 新增：清零标记
            
            // 子进程
            int ret = m_func();
            exit(ret);  // 子进程必须显式退出
        }
        // 父进程
        close(m_ipc_sockets[0]);      // ← 新增：关闭读端
        m_ipc_sockets[0] = 0;         // ← 新增：清零标记
        // 父进程：关闭读端，只保留写端用于发送
        // 原因：1.父进程只需发送fd，不需接收
        //      2.节省资源，防止fd泄漏
        //      3.明确单向通信设计
        // 父进程
        m_pid = pid;
        return 0;
    }

    // 等待子进程结束
    int Wait(int* status = nullptr) {
        if (m_pid <= 0) {
            return -1;
        }

        int stat;
        pid_t ret = waitpid(m_pid, &stat, 0);
        if (ret == -1) {
            perror("waitpid");
            return -1;
        }

        if (status) {
            *status = stat;
        }

        printf("[父进程] 子进程 %d 已结束\n", m_pid);

        if (WIFEXITED(stat)) {
            printf("  正常退出，退出码: %d\n", WEXITSTATUS(stat));
        }
        else if (WIFSIGNALED(stat)) {
            printf("  被信号终止，信号: %d\n", WTERMSIG(stat));
        }

        return 0;
    }
    // 父进程发送文件描述符给子进程
    int SendFD(int fd) {

        /*  参数：int fd
          - 要传递给子进程的文件描述符
          - 可以是任何类型的fd：socket、文件、管道等
          - 这个fd必须在父进程中是有效的

          返回值：int
          - 0：成功
          - -1：calloc失败（内存分配失败）
          - -2：sendmsg失败（发送失败）*/
        struct msghdr msg;
        memset(&msg, 0, sizeof(msg));
        /*  msghdr 结构体是什么？

          struct msghdr {
              void*         msg_name;       // 目标地址（我们不用）
              socklen_t     msg_namelen;    // 地址长度

              struct iovec* msg_iov;        // 【数据块数组】
              size_t        msg_iovlen;     // 数据块数量

              void*         msg_control;    // 【控制消息】← 这里放fd
              size_t        msg_controllen; // 控制消息长度

              int           msg_flags;      // 标志位
          };
         */ 
         // 准备普通数据（可选，这里作为示例）
        struct iovec iov[2];
        iov[0].iov_base = (void*)"edoyun";
        iov[0].iov_len = 7;
        iov[1].iov_base = (void*)"jueding";
        iov[1].iov_len = 8;

        msg.msg_iov = iov;
        msg.msg_iovlen = 2;

        // 分配控制消息内存// 分配控制消息内存（必须用calloc清零，防止随机值）
        struct cmsghdr* cmsg = (struct cmsghdr*)calloc(1, CMSG_LEN(sizeof(int)));
        if (cmsg == nullptr) {
            perror("calloc");
            return -1;
        }
        /*
        
          1. cmsghdr 是什么？

          struct cmsghdr {
              socklen_t cmsg_len;    // 控制消息总长度（含头部）
              int       cmsg_level;  // 协议层级
              int       cmsg_type;   // 消息类型
              // 后面是数据区（通过CMSG_DATA宏访问）
          };

          结构示意：
          ┌──────────────────────────────┐
          │  cmsg_len = 20               │  ← 头部
          │  cmsg_level = SOL_SOCKET     │
          │  cmsg_type = SCM_RIGHTS      │
          ├──────────────────────────────┤
          │  数据: fd值（4字节）          │  ← 数据区
          └──────────────────────────────┘

          作用：
          - 这是"控制消息"的包装
          - 普通数据走 msg_iov（我们前面设置的）
          - 特殊数据走 msg_control（文件描述符就在这里）

          --- 
         3. CMSG_LEN 宏的作用

          CMSG_LEN(sizeof(int))
          //       ^^^^^^^^^^^
          //       数据区大小（4字节，一个int）

          宏的计算：
          // CMSG_LEN的定义（简化版）
          #define CMSG_LEN(len) (sizeof(struct cmsghdr) + (len))
          //                     ^^^^^^^^^^^^^^^^^^^^^   ^^^^^
          //                     头部大小（12-16字节）    数据大小（4字节）

          // 实际计算：
          CMSG_LEN(sizeof(int))
          = sizeof(struct cmsghdr) + 4
          = 16 + 4  // 假设64位系统
          = 20字节

          为什么不直接写 20？
          // ❌ 错误：硬编码
          calloc(1, 20);  // 不同平台可能不同

          // ✅ 正确：用宏
          calloc(1, CMSG_LEN(sizeof(int)));  // 自动适配平台

          平台差异：
          32位系统：cmsghdr可能是12字节
          64位系统：cmsghdr可能是16字节

          用宏就不用担心这些差异
      
        */
        // 设置控制消息长度
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));

        // 设置协议层级为socket层
        cmsg->cmsg_level = SOL_SOCKET;

        // 设置消息类型为传递访问权限（文件描述符）
        cmsg->cmsg_type = SCM_RIGHTS;
        /*  SCM_RIGHTS 的含义：
          SCM = Socket Control Message（socket控制消息）
          RIGHTS = 访问权限

          合起来：传递访问权限（即文件描述符）

          这是触发内核魔法的关键！

          内核看到 SCM_RIGHTS 后的行为：
          1. 识别：这不是普通数据，是要传递文件描述符
          2. 查找：在发送进程的fd表中查找这个fd对应的内核对象
          3. 分配：在接收进程的fd表中分配一个新的fd
          4. 映射：让新fd也指向同一个内核对象
          5. 传递：把新分配的fd值发送给接收进程*/

        // 将文件描述符写入控制消息的数据区
        *(int*)CMSG_DATA(cmsg) = fd;
        /*  *(int*)CMSG_DATA(cmsg) = fd;
          //^^^^^ ^^^^^^^^^^^^^   ^^
          //强转  获取数据区指针   要传递的fd

          CMSG_DATA 宏的作用：
          // 宏的定义（简化版）
          #define CMSG_DATA(cmsg) ((unsigned char*)(cmsg) + sizeof(struct cmsghdr))
          //                       ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
          //                       跳过头部，指向数据区

          内存布局：
          cmsg 指针 ──────> ┌──────────────────┐
                            │ cmsg_len         │  \
                            │ cmsg_level       │   } 头部（cmsghdr）
                            │ cmsg_type        │  /
          CMSG_DATA(cmsg)─> ├──────────────────┤
                            │ fd值（4字节）     │  ← 数据区
                            └──────────────────┘

          为什么要强转？
          CMSG_DATA(cmsg)  // 返回类型：unsigned char*（字节指针）
          *(int*)          // 强转为 int*，然后解引用

          // 完整过程：
          unsigned char* ptr = CMSG_DATA(cmsg);  // 获取数据区指针
          int* fd_ptr = (int*)ptr;                // 转换为int指针
          *fd_ptr = fd;                           // 写入fd值

          // 简化写法：
          *(int*)CMSG_DATA(cmsg) = fd;*/
        // 将控制消息关联到消息头
        msg.msg_control = cmsg;
        msg.msg_controllen = cmsg->cmsg_len;

        // 通过socket发送消息（普通数据+控制消息）
        ssize_t ret = sendmsg(m_ipc_sockets[1], &msg, 0);

        // 释放临时内存
        free(cmsg);

        // 检查发送结果
        if (ret == -1) {
            perror("sendmsg");
            return -2;
        }
        return 0;
    }
    // 子进程接收父进程发送的文件描述符
    int RecvFD(int& fd) {
        struct msghdr msg;
        memset(&msg, 0, sizeof(msg));

        // 接收普通数据
        struct iovec iov[2];
        char buf[2][10] = { "", "" };
        iov[0].iov_base = buf[0];
        iov[0].iov_len = sizeof(buf[0]);
        iov[1].iov_base = buf[1];
        iov[1].iov_len = sizeof(buf[1]);
        msg.msg_iov = iov;
        msg.msg_iovlen = 2;

        // 准备接收控制消息
        struct cmsghdr* cmsg = (struct cmsghdr*)calloc(1, CMSG_LEN(sizeof(int)));
        if (cmsg == nullptr) {
            perror("calloc");
            return -1;
        }

        cmsg->cmsg_len = CMSG_LEN(sizeof(int));
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        msg.msg_control = cmsg;
        msg.msg_controllen = CMSG_LEN(sizeof(int));

        // 接收（阻塞等待）
        ssize_t ret = recvmsg(m_ipc_sockets[0], &msg, 0);
        if (ret == -1) {
            perror("recvmsg");
            free(cmsg);
            return -2;
        }

        // 提取fd
        fd = *(int*)CMSG_DATA(cmsg);
        free(cmsg);

        return 0;
    }

    static int SwitchDaemon() {
        // 步骤1：第一次fork - 确保不是进程组长
        pid_t pid = fork();
        if (pid == -1) {
            perror("第一次fork失败");
            return -1;
        }
        if (pid > 0) {
            exit(0);  // 父进程退出
        }
        // 步骤2：setsid - 创建新会话，脱离控制终端
        if (setsid() == -1) {
            perror("setsid失败");
            return -2;
        }
        // 步骤3：第二次fork - 确保不是会话首进程
        pid = fork();
        if (pid == -1) {
            perror("第二次fork失败");
            return -3;
        }
        if (pid > 0) {
            exit(0);  // 子进程退出
        }
        // 步骤4：关闭并重定向标准文件描述符
        for (int i = 0; i < 3; i++) {
            close(i);
        }

        int fd = open("/dev/null", O_RDWR);
        if (fd != -1) {
            dup2(fd, STDIN_FILENO);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            if (fd > 2) {
                close(fd);
            }
        }

        // 步骤5：重置umask
        umask(0);

        // 步骤6：忽略SIGCHLD信号
        signal(SIGCHLD, SIG_IGN);

        return 0;
    }

    pid_t GetPid() const { return m_pid; }

private:
    std::function<int()> m_func;
    pid_t m_pid{ -1 };
    int m_ipc_sockets[2]{ 0,0 };

};

int CreateLogServer(CProcess* proc)
{
    // 第1步：创建日志服务器对象
    CLoggerServer server;

    // 第2步：启动日志服务器
    int ret = server.Start();
    if (ret != 0) {
        printf("%s(%d):<%s> pid=%d errno:%d msg:%s ret:%d\n",
            __FILE__, __LINE__, __FUNCTION__, getpid(),
            errno, strerror(errno), ret);
        return ret;
    }

    // 第3步：阻塞等待（接收父进程的控制信号）
    int fd = 0;
    while (true) {
        ret = proc->RecvFD(fd);
        printf("%s(%d):<%s> fd=%d\n", __FILE__, __LINE__, __FUNCTION__, fd);
        if (fd <= 0) break;  // 接收到-1表示父进程要求退出
    }

    // 第4步：关闭日志服务器
    ret = server.Close();
    printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    return 0;
}

int CreateClientServer(CProcess* proc)
{
   /* printf("[子进程 %d] 客户端服务器启动\n", getpid());

    // 接收父进程发送的文件描述符
    int received_fd = -1;
    int ret = proc->RecvFD(received_fd);
    if (ret != 0) {
        printf("[子进程] 接收fd失败: %d\n", ret);
        return -1;
    }

    printf("[子进程] 接收到文件描述符: %d\n", received_fd);

    // 测试：从接收到的fd读取数据
    char buffer[100] = { 0 };
    ssize_t n = read(received_fd, buffer, sizeof(buffer) - 1);
    if (n > 0) {
        printf("[子进程] 从fd=%d读取到数据: %s\n", received_fd, buffer);
    }

    close(received_fd);
    printf("[子进程] 客户端服务器退出\n");
    return 0;*/
    printf("[子进程 %d] 客户端服务器启动\n", getpid());

    // 接收fd
    int received_fd = -1;
    int ret = proc->RecvFD(received_fd);
    if (ret != 0) {
        printf("[子进程] 接收fd失败: %d\n", ret);
        return -1;
    }
    printf("[子进程] 接收到文件描述符: %d\n", received_fd);

    // 【新增】等待父进程写入
    printf("[子进程] 等待父进程写入数据...\n");
    sleep(1);

    // 【新增】移动文件指针到开头
    off_t pos = lseek(received_fd, 0, SEEK_SET);
    if (pos == -1) {
        perror("lseek 失败");
        close(received_fd);
        return -2;
    }
    // 读取数据
    char buffer[100] = { 0 };
    ssize_t n = read(received_fd, buffer, sizeof(buffer) - 1);
    if (n > 0) {
        buffer[n] = '\0';
        printf("[子进程] 从fd=%d读取到数据: '%s' (长度=%zd)\n",
            received_fd, buffer, n);
    }
    else if (n == 0) {
        printf("[子进程] 文件为空\n");
    }
    else {
        perror("[子进程] read失败");
    }

    close(received_fd);
    printf("[子进程] 客户端服务器退出\n");
    return 0;
}
int TestEpoll()
{
    printf("=== Epoll 测试程序 ===\n");

    // 1. 创建 epoll
    CEpoll epoll;
    int ret = epoll.Create(10);
    if (ret != 0) {
        fprintf(stderr, "创建 epoll 失败: %d\n", ret);
        return -1;
    }
    printf("✓ Epoll 创建成功\n");

    // 2. 监听标准输入（fd=0）
    ret = epoll.Add(STDIN_FILENO, EpollData(STDIN_FILENO), EPOLLIN);
    if (ret != 0) {
        fprintf(stderr, "添加监听失败: %d\n", ret);
        return -2;
    }
    printf("✓ 开始监听键盘输入\n");
    printf("  请输入文字（按回车发送，输入 quit 退出）:\n\n");

    // 3. 事件循环
    EPEvents events;
    while (true) {
        // 等待事件（超时5秒）
        ssize_t n = epoll.WaitEvents(events, 5000);

        if (n < 0) {
            fprintf(stderr, "WaitEvents 失败: %zd\n", n);
            break;
        }

        if (n == 0) {
            printf("  [超时，没有输入]\n");
            continue;
        }

        // 处理事件
        for (int i = 0; i < n; i++) {
            epoll_event& ev = events[i];

            if (ev.events & EPOLLIN) {
                // 可读事件
                char buf[256] = { 0 };
                ssize_t len = read(ev.data.fd, buf, sizeof(buf) - 1);

                if (len > 0) {
                    buf[len] = '\0';
                    // 去掉换行符
                    if (buf[len - 1] == '\n') buf[len - 1] = '\0';

                    printf("  收到输入: [%s]\n", buf);

                    // 退出条件
                    if (strcmp(buf, "quit") == 0) {
                        printf("\n再见！\n");
                        return 0;
                    }
                }
            }
        }
    }

    return 0;
}

// ==========================================
// 线程测试函数
// ==========================================

// 测试任务1：简单的计数任务
int SimpleTask(int count) {
    printf("[任务1] 线程启动，参数: %d\n", count);

    for (int i = 0; i < count; i++) {
        printf("[任务1] 工作中... %d/%d\n", i + 1, count);
        sleep(1);
    }

    printf("[任务1] 任务完成！\n");
    return 0;
}

// 测试任务2：可暂停的任务
int PausableTask() {
    printf("[任务2] 线程启动\n");

    for (int i = 0; i < 20; i++) {
        printf("[任务2] 工作中... %d/20\n", i + 1);
        sleep(1);
    }

    printf("[任务2] 任务完成！\n");
    return 0;
}

int TestThread()
{
    printf("=== 线程封装测试程序（简化版）===\n\n");

    // ==========================================
    // 简化测试：只测试暂停功能
    // ==========================================
    printf("【简化测试】暂停和恢复\n");
    printf("----------------------------------------\n");

    CThread t2(PausableTask);

    printf("[主线程] 启动线程...\n");
    int ret = t2.Start();
    if (ret != 0) {
        printf("[主线程] 启动失败: %d\n", ret);
        return -2;
    }
    printf("[主线程] Start() 完成\n");

    printf("[主线程] 等待1秒，确保信号注册完成...\n");  // ← 新增
    sleep(1);  // ← 新增：等待信号注册

    printf("[主线程] 让线程运行3秒...\n");
    sleep(3);

    printf("[主线程] 准备暂停线程！\n");
    ret = t2.Pause();
    if (ret != 0) {
        printf("[主线程] 暂停失败: %d\n", ret);
        return -3;
    }
    printf("[主线程] Pause() 调用成功\n");

    printf("[主线程] 线程已暂停，等待3秒...\n");
    sleep(3);

    printf("[主线程] 准备恢复线程！\n");
    ret = t2.Pause();  // 再次调用 Pause 表示恢复
    if (ret != 0) {
        printf("[主线程] 恢复失败: %d\n", ret);
        return -4;
    }
    printf("[主线程] 线程已恢复\n");

    printf("[主线程] 让线程继续运行3秒...\n");
    sleep(3);

    printf("[主线程] 准备停止线程...\n");
    ret = t2.Stop();
    if (ret != 0) {
        printf("[主线程] 停止失败: %d\n", ret);
    }
    printf("[主线程] Stop() 完成\n");

    printf("\n=== 测试完成了2333！===\n");
    return 0;
}
int LogTest()
{
    char buffer[] = "hello 王万鑫! 我是王万鑫";
    usleep(1000 * 100);
    TRACEI("here is log %d %c %f %g %s 哈哈 嘻嘻 我是王万鑫", 10, 'A', 1.0f, 2.0,
        buffer);
    DUMPD((void*)buffer, (size_t)sizeof(buffer));
    LOGE << 100 << " " << 'S' << " " << 0.12345f << " " << 1.23456789 << " " <<
        buffer << " 王万鑫";
    return 0;
}

// ==========================================
// 线程池测试函数
// ==========================================

// 测试任务1：简单打印
void Task1() {
    printf("[任务1] 线程ID: %lu, 执行简单任务\n", pthread_self());
}

// 测试任务2：带参数的任务
void Task2(int id, const char* msg) {
    printf("[任务2] 线程ID: %lu, ID=%d, 消息=%s\n", pthread_self(), id, msg);
}

// 测试任务3：计算任务
int Task3(int a, int b) {
    int result = a + b;
    printf("[任务3] 线程ID: %lu, %d + %d = %d\n", pthread_self(), a, b, result);
    return result;
}

// 测试任务4：耗时任务
void Task4(int seconds) {
    printf("[任务4] 线程ID: %lu, 开始执行，将耗时%d秒\n", pthread_self(), seconds);
    sleep(seconds);
    printf("[任务4] 线程ID: %lu, 执行完成\n", pthread_self());
}

// 阶段1：基础功能测试
int TestThreadPool_Basic() {
    printf("\n========================================\n");
    printf("  阶段1：线程池基础功能测试\n");
    printf("========================================\n\n");

    // 步骤1：创建线程池
    printf("【步骤1】创建线程池...\n");
    CThreadPool pool;
    printf("✓ 线程池对象创建成功\n\n");

    // 步骤2：启动线程池（4个工作线程）
    printf("【步骤2】启动线程池（4个工作线程）...\n");
    int ret = pool.Start(4);
    if (ret != 0) {
        printf("❌ 启动失败，错误码: %d\n", ret);
        if (ret != 0) printf("  errno:%d msg:%s\n", errno, strerror(errno));
        return -1;
    }
    printf("✓ 线程池启动成功\n\n");

    // 步骤3：提交简单任务
    printf("【步骤3】提交4个简单任务...\n");
    for (int i = 0; i < 4; i++) {
        ret = pool.AddTask(Task1);
        if (ret != 0) {
            printf("❌ 提交任务%d失败，错误码: %d\n", i + 1, ret);
        } else {
            printf("✓ 任务%d已提交\n", i + 1);
        }
    }
    printf("\n");

    // 步骤4：等待任务执行
    printf("【步骤4】等待任务执行...\n");
    sleep(2);
    printf("✓ 任务执行完成\n\n");

    // 步骤5：关闭线程池
    printf("【步骤5】关闭线程池...\n");
    pool.Close();
    printf("✓ 线程池已关闭\n\n");

    printf("========================================\n");
    printf("  ✅ 阶段1测试通过！\n");
    printf("========================================\n\n");

    return 0;
}

// 阶段2：进阶功能测试
int TestThreadPool_Advanced() {
    printf("\n========================================\n");
    printf("  阶段2：线程池进阶功能测试\n");
    printf("========================================\n\n");

    CThreadPool pool;
    int ret = pool.Start(4);
    if (ret != 0) {
        printf("❌ 启动失败: %d\n", ret);
        return -1;
    }
    printf("✓ 线程池启动成功\n\n");

    // 测试1：带参数的任务
    printf("【测试1】提交带参数的任务...\n");
    pool.AddTask(Task2, 1, "第一个任务");
    pool.AddTask(Task2, 2, "第二个任务");
    pool.AddTask(Task2, 3, "第三个任务");
    sleep(1);
    printf("\n");

    // 测试2：Lambda表达式
    printf("【测试2】提交Lambda表达式...\n");
    pool.AddTask([]() {
        printf("[Lambda任务] 线程ID: %lu, 无捕获Lambda\n", pthread_self());
    });

    pool.AddTask([](int x) {
        printf("[Lambda任务] 线程ID: %lu, 参数x=%d\n", pthread_self(), x);
    }, 42);

    int value = 100;
    pool.AddTask([value]() {
        printf("[Lambda任务] 线程ID: %lu, 捕获value=%d\n", pthread_self(), value);
    });
    sleep(1);
    printf("\n");

    // 测试3：计算任务
    printf("【测试3】提交计算任务...\n");
    pool.AddTask(Task3, 10, 20);
    pool.AddTask(Task3, 5, 15);
    sleep(1);
    printf("\n");

    pool.Close();
    printf("✓ 线程池已关闭\n\n");

    printf("========================================\n");
    printf("  ✅ 阶段2测试通过！\n");
    printf("========================================\n\n");

    return 0;
}

// 阶段3：压力测试
int TestThreadPool_Stress() {
    printf("\n========================================\n");
    printf("  阶段3：线程池压力测试\n");
    printf("========================================\n\n");

    CThreadPool pool;
    int ret = pool.Start(4);
    if (ret != 0) {
        printf("❌ 启动失败: %d\n", ret);
        return -1;
    }
    printf("✓ 线程池启动成功（4个工作线程）\n\n");

    // 测试1：批量提交
    printf("【测试1】批量提交100个任务...\n");
    for (int i = 0; i < 100; i++) {
        pool.AddTask([i]() {
            printf("[批量任务] 任务%d 在线程%lu执行\n", i, pthread_self());
        });
    }
    printf("✓ 100个任务已提交\n");
    printf("  等待执行...\n");
    sleep(5);
    printf("✓ 批量任务执行完成\n\n");

    // 测试2：混合任务
    printf("【测试2】提交混合任务（快速+耗时）...\n");
    pool.AddTask(Task1);  // 快速任务
    pool.AddTask(Task4, 2);  // 耗时2秒
    pool.AddTask(Task1);  // 快速任务
    pool.AddTask(Task4, 1);  // 耗时1秒
    printf("  等待所有任务完成...\n");
    sleep(3);
    printf("✓ 混合任务执行完成\n\n");

    pool.Close();
    printf("✓ 线程池已关闭\n\n");

    printf("========================================\n");
    printf("  ✅ 阶段3测试通过！\n");
    printf("========================================\n\n");

    return 0;
}

// 完整测试套件
int TestThreadPool_All() {
    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║     线程池完整测试套件                  ║\n");
    printf("╚════════════════════════════════════════╝\n");

    int ret = 0;

    // 阶段1
    ret = TestThreadPool_Basic();
    if (ret != 0) {
        printf("\n❌ 阶段1测试失败\n");
        return -1;
    }
    printf("按回车继续阶段2测试...\n");
    getchar();

    // 阶段2
    ret = TestThreadPool_Advanced();
    if (ret != 0) {
        printf("\n❌ 阶段2测试失败\n");
        return -2;
    }
    printf("按回车继续阶段3测试...\n");
    getchar();

    // 阶段3
    ret = TestThreadPool_Stress();
    if (ret != 0) {
        printf("\n❌ 阶段3测试失败\n");
        return -3;
    }

    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║     🎉 所有测试通过！                   ║\n");
    printf("╚════════════════════════════════════════╝\n");
    printf("\n");

    return 0;
}

int main()
{
#pragma region 第一日测试
    //printf("[父进程 %d] 开始创建子进程\n", getpid());

   //CProcess proclog, procclients;

   //// 创建日志服务器进程
   //proclog.SetEntryFunction(CreateLogServer, &proclog);
   //int ret = proclog.CreateSubProcess();
   //if (ret != 0) {
   //    fprintf(stderr, "创建日志服务器进程失败: %d\n", ret);
   //    return ret;
   //}
   //printf("[父进程] 创建日志服务器进程成功，PID: %d\n", proclog.GetPid());

   //// 创建客户端服务器进程
   //procclients.SetEntryFunction(CreateClientServer, &procclients);
   //ret = procclients.CreateSubProcess();
   //if (ret != 0) {
   //    fprintf(stderr, "创建客户端服务器进程失败: %d\n", ret);
   //    return ret;
   //}
   //printf("[父进程] 创建客户端服务器进程成功，PID: %d\n", procclients.GetPid());

   ////// 等待所有子进程结束
   ////printf("\n[父进程] 等待子进程结束...\n\n");
   ////proclog.Wait();
   ////procclients.Wait();

   ////printf("\n[父进程] 所有子进程已结束，父进程退出\n");
   //return 0;


#pragma endregion

#pragma region 测试pipe 
   /* printf("[父进程 %d] 开始测试文件描述符传递\n", getpid());

    CProcess procclients;

    // 创建客户端服务器进程
    procclients.SetEntryFunction(CreateClientServer, &procclients);
    int ret = procclients.CreateSubProcess();
    if (ret != 0) {
        fprintf(stderr, "创建客户端服务器进程失败: %d\n", ret);
        return ret;
    }
    printf("[父进程] 创建子进程成功，PID: %d\n", procclients.GetPid());

    // 测试：创建一个pipe，把读端发送给子进程
    int test_pipe[2];
    if (pipe(test_pipe) == -1) {
        perror("pipe");
        return -1;
    }
    printf("[父进程] 创建pipe成功: 读端=%d, 写端=%d\n", test_pipe[0],
        test_pipe[1]);

    // 发送读端给子进程
    ret = procclients.SendFD(test_pipe[0]);
    if (ret != 0) {
        fprintf(stderr, "[父进程] 发送fd失败: %d\n", ret);
        return -1;
    }
    printf("[父进程] 已发送fd=%d给子进程\n", test_pipe[0]);

    // 父进程不需要读端，关闭
    close(test_pipe[0]);

    // 向pipe写入数据
    const char* msg = "Hello from parent!";
    write(test_pipe[1], msg, strlen(msg));
    printf("[父进程] 已写入数据: %s\n", msg);

    // 关闭写端
    close(test_pipe[1]);

    // 等待子进程结束
    procclients.Wait();

    printf("\n[父进程] 测试完成\n");
    return 0;*/
#pragma endregion

#pragma region 测试文件传递文件描述符
//  printf("[父进程 %d] 程序启动\n", getpid());
//  CProcess proclog;
//  proclog.SetEntryFunction(CreateLogServer, &proclog);
//  int ret = proclog.CreateSubProcess();
//  if (ret != 0) {
//      fprintf(stderr, "创建日志服务器失败: %d\n", ret);
//      return -1;
//  }
//  printf("[父进程] 日志服务器PID: %d\n", proclog.GetPid());
//
// // ─────────────────────────────
//// 第2步：创建客户端服务器进程
//// ─────────────────────────────
//  CProcess procclients;
//  procclients.SetEntryFunction(CreateClientServer, &procclients);
//  ret = procclients.CreateSubProcess();
//  if (ret != 0) {
//      fprintf(stderr, "创建客户端服务器失败: %d\n", ret);
//      return -2;
//  }
//  printf("[父进程] 客户端服务器PID: %d\n", procclients.GetPid());
//
// // ─────────────────────────────
//// 第3步：等待100毫秒
//// ─────────────────────────────
//  printf("[父进程] 等待子进程初始化...\n");
//  usleep(100 * 1000);
//
//
//  // ─────────────────────────────
//  // 第4步：打开文件
//  // ─────────────────────────────
//
//  int fd = open("./text.txt", O_RDWR | O_CREAT | O_APPEND, 0644);
//  if (fd == -1) {
//      perror("open失败");
//      return -3;
//  }
//  printf("[父进程] 文件fd=%d\n", fd);
//
//  // ─────────────────────────────
//  // 第5步：发送fd给子进程
//  // ─────────────────────────────
//  printf("[父进程] 发送fd...\n");
//  ret = procclients.SendFD(fd);
//  if (ret != 0) {
//      fprintf(stderr, "SendFD失败: %d\n", ret);
//      close(fd);
//      return -4;
//  }
//
//  // ─────────────────────────────
//  // 第6步：写入数据
//  // ─────────────────────────────
//  printf("[父进程] 写入数据...\n");
//  write(fd, "edoyun", 6);
//
//  // ─────────────────────────────
//  // 第7步：关闭文件
//  // ─────────────────────────────
//  close(fd);
//  printf("[父进程] 已关闭文件\n");
//
//  // ─────────────────────────────
//  // 第8步：等待子进程结束
//  // ─────────────────────────────
//  printf("[父进程] 等待子进程...\n");
//  proclog.Wait();
//  procclients.Wait();
//
//  printf("[父进程] 测试完成\n");
//  return 0;
#pragma endregion

#pragma region 测试守护进程
//   // 转为守护进程
//if (CProcess::SwitchDaemon() != 0) {
//    fprintf(stderr, "守护进程初始化失败\n");
//    return -1;
//}
//
//// 守护进程逻辑
//// 注意：printf不会显示，需要写日志文件
//int log_fd = open("/tmp/daemon_test.log",
//    O_CREAT | O_WRONLY | O_APPEND, 0644);
//
//for (int i = 0; i < 100; i++) {
//    char buf[100];
//    snprintf(buf, sizeof(buf), "守护进程运行中 %d\n", i);
//    write(log_fd, buf, strlen(buf));
//    sleep(2);
//}
//
//close(log_fd);
//return 0;

#pragma endregion

#pragma region testepoll
    //return TestEpoll();
#pragma endregion

#pragma region 测试线程封装
   // return TestThread();
#pragma endregion

#pragma region 测试线程池
    // 测试线程池（不需要日志服务器）
    return TestThreadPool_All();
#pragma endregion

std::cout << "hello" << std::endl;
CProcess proclog;

// 第1步：创建日志服务器子进程
proclog.SetEntryFunction(CreateLogServer, &proclog);
int ret = proclog.CreateSubProcess();
if (ret != 0) {
    printf("创建日志服务器失败: %d\n", ret);
    return -1;
}

// 第2步：调用 LogTest（此时才能输出日志）
LogTest();

// ⚡ 第2.5步：等待日志写入完成（重要！）
printf("[父进程] 等待日志写入文件...\n");
sleep(1);  // 给日志服务器1秒时间处理

// 第3步：等待用户输入
printf("[父进程] 日志已写入，按回车键结束程序...\n");
getchar();  // 按任意键退出

// ⚡ 第4步：发送退出信号给日志服务器子进程
printf("[父进程] 正在发送退出信号...\n");
ret = proclog.SendFD(-1);  // ← 发送 -1 表示退出
if (ret != 0) {
    printf("[父进程] 发送退出信号失败: %d\n", ret);
}

// 第5步：等待子进程退出（可选）
printf("[父进程] 等待日志服务器关闭...\n");
sleep(1);  // 给子进程一点时间清理资源

printf("[父进程] 程序结束\n");
return 0;

}
