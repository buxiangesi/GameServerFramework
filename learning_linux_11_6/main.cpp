#include <cstdio>
#include <unistd.h>
#include <functional>
#include <memory>
#include <utility>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/socket.h>  // ← 新增
#include <cstring>       // ← 新增
#include <fcntl.h>       // ← 新增
#include <sys/stat.h>    // ← 新增
#include "EpollData.h"
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
    printf("[子进程 %d] 日志服务器启动\n", getpid());

    // 模拟工作
    for (int i = 0; i < 3; i++) {
        printf("[日志服务器 %d] 工作中... %d\n", getpid(), i);
        sleep(1);
    }

    printf("[子进程 %d] 日志服务器退出\n", getpid());
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
       return TestEpoll();
#pragma endregion

}
