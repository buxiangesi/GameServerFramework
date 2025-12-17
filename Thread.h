#pragma once
#include <pthread.h>     // pthread_create, pthread_join 等
#include <signal.h>      // sigaction, SIGUSR1, SIGUSR2
#include <unistd.h>      // usleep
#include <functional>    // std::function
#include <map>           // std::map（管理线程）
#include<cstdio>
#include <errno.h>   // ETIMEDOUT
#include <time.h>    // timespec
#include <memory.h>
class CThread
{
public:
    // 默认构造函数
    CThread()
        : m_thread(0), m_bpaused(false)
    {
    }

    // 带参数的构造函数（模板）
    template<typename F, typename... Args>
    CThread(F&& func, Args&&... args)
        : m_thread(0), m_bpaused(false)
    {
        // 使用 std::bind 绑定函数和参数（支持成员函数指针）
        auto bound = std::bind(std::forward<F>(func), std::forward<Args>(args)...);

        // 封装到 std::function 中
        m_function = [bound]() mutable -> int {
            bound();  // 调用绑定的函数
            return 0;
        };
    }

    // 析构函数
    ~CThread() {
        // 线程结束时的清理工作（后面实现）
    }

    // 禁用拷贝构造和拷贝赋值
    CThread(const CThread&) = delete;
    CThread& operator=(const CThread&) = delete;

    template<typename F, typename... Args>
    int SetThreadFunc(F&& func, Args&&... args) {
        // 使用 std::bind 绑定函数和参数（支持成员函数指针）
        auto bound = std::bind(std::forward<F>(func), std::forward<Args>(args)...);

        // 封装到 std::function 中
        m_function = [bound]() mutable -> int {
            bound();  // 调用绑定的函数
            return 0;
        };
        return 0;
    }
    // 检查线程状态
    bool isValid() const {
        return m_thread != 0;
    }
    int Start() {
        //回顾：pthread_create API

            //int pthread_create(
            //    pthread_t * thread,              // 输出：线程ID
            //    const pthread_attr_t * attr,     // 线程属性（可为NULL）
            //    void* (*start_routine)(void*),  // 线程入口函数
            //    void* arg                       // 传给入口函数的参数
            //);
        // 1. 检查任务函数
        if (!m_function) return -1;

        // 2. 检查线程状态
        if (m_thread != 0) return -2;

        // 3. 初始化线程属性
        pthread_attr_t attr;
        int ret = 0;

        ret = pthread_attr_init(&attr);
        if (ret != 0) return -3;

        // 4. 设置为可join状态
        ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
        if (ret != 0) return -4;

        // 5. 设置系统级竞争
        ret = pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
        if (ret != 0) return -5;

        // 6. 创建线程
        ret = pthread_create(&m_thread, &attr, &CThread::ThreadEntry, this);
        if (ret != 0) return -6;

        // 7. 注册到静态map（用于信号处理）
        m_mapThread[m_thread] = this;
        printf("[调试] 线程 %lu 插入到 map\n", (unsigned long)m_thread);  // ← 调试输出

        // 8. 销毁属性对象
        ret = pthread_attr_destroy(&attr);
        if (ret != 0) return -7;

        return 0;
    }
    int Stop() {
        // 第1步：检查线程是否存在
        if (m_thread != 0) {

            // 第2步：保存线程ID，清零成员变量
            pthread_t thread = m_thread;
            m_thread = 0;

            // 第3步：设置超时时间（100ms）
            timespec ts;
            ts.tv_sec = 0;               // 秒数
            ts.tv_nsec = 100 * 1000000;  // 纳秒数（100ms）

            // 第4步：等待线程结束（带超时）
            int ret = pthread_timedjoin_np(thread, NULL, &ts);

            // 第5步：如果超时，强制终止
            if (ret == ETIMEDOUT) {
                pthread_detach(thread);       // 先 detach
                pthread_kill(thread, SIGUSR2); // 再发信号
            }
        }
        return 0;
    }
    int Pause() {
        // 第1步：检查线程是否存在
        if (m_thread == 0) return -1;  // 线程不存在，无法暂停

        // 第2步：如果已经暂停，就恢复
        if (m_bpaused) {
            printf("[调试] 恢复线程\n");  // ← 调试输出
            m_bpaused = false;  // 改标志就行，不需要发信号
            return 0;
        }

        // 第3步：设置暂停标志
        printf("[调试] 暂停线程 %lu\n", (unsigned long)m_thread);  // ← 调试输出
        m_bpaused = true;

        // 第4步：发送信号通知线程
        int ret = pthread_kill(m_thread, SIGUSR1);
        if (ret != 0) {
            printf("[调试] pthread_kill 失败: %d\n", ret);  // ← 调试输出
            m_bpaused = false;  // 发送失败，恢复标志
            return -2;
        }
        printf("[调试] SIGUSR1 信号已发送\n");  // ← 调试输出

        return 0;
    }
   
private:
    // 静态线程入口函数
    static void* ThreadEntry(void* arg) {
        // 1. 转换参数为对象指针
        CThread* thiz = (CThread*)arg;
        printf("[调试-ThreadEntry] 线程入口开始执行\n");  // ← 新增

        // TODO: 2. 设置信号处理（稍后实现）

              // ========== 信号注册（5步） ==========

      // 第1步：创建 sigaction 结构体
        printf("[调试-ThreadEntry] 准备注册信号处理函数\n");  // ← 新增
        struct sigaction act;
        memset(&act, 0, sizeof(act));  // 清零

        // 第2步：清空信号屏蔽集
        sigemptyset(&act.sa_mask);// 不屏蔽任何信号
        // 意思是：处理信号时，其他信号可以中断

        // 第3步：设置标志（使用 sa_sigaction 而不是 sa_handler）
        act.sa_flags = SA_SIGINFO;
        //SA_SIGINFO 的含义：
        //    - 使用 sa_sigaction 字段（3个参数的版本）
        //    - 而不是 sa_handler 字段（1个参数的版本）

        // 第4步：指定处理函数（关键！）
        act.sa_sigaction = &CThread::Sigaction;  // ← 这里指定了 CThread::Sigaction！

        // 第5步：注册两个信号（添加错误检查）
        if (sigaction(SIGUSR1, &act, NULL) != 0) {
            printf("[错误] SIGUSR1 注册失败\n");
        } else {
            printf("[调试-ThreadEntry] SIGUSR1 注册成功\n");  // ← 新增
        }
        if (sigaction(SIGUSR2, &act, NULL) != 0) {
            printf("[错误] SIGUSR2 注册失败\n");
        } else {
            printf("[调试-ThreadEntry] SIGUSR2 注册成功\n");  // ← 新增
        }
        printf("[调试-ThreadEntry] 信号注册完成，开始执行用户任务\n");  // ← 新增

        // ========== 执行用户任务 ==========
        thiz->EnterThread();

        // ========== 清理工作 ==========
        if (thiz->m_thread) {
            thiz->m_thread = 0;
        }

        pthread_t thread = pthread_self();
        auto it = m_mapThread.find(thread);
        if (it != m_mapThread.end()) {
            m_mapThread[thread] = nullptr;
        }

        pthread_detach(thread);
        pthread_exit(NULL);
    }

    void EnterThread() {
        if (m_function) {
            int ret = m_function();
            if (ret != 0) {
                printf("%s(%d):[%s] ret = %d\n",
                    __FILE__, __LINE__, __FUNCTION__, ret);
            }
        }
    }
    static void Sigaction(int signo, siginfo_t* info, void* context)
    {
        printf("[调试] Sigaction 被调用，信号=%d\n", signo);  // ← 调试输出

        if (signo == SIGUSR1) {
            // 【第1步】获取当前线程ID
            pthread_t thread = pthread_self();  // 例如：12346
            printf("[调试] 当前线程ID: %lu\n", (unsigned long)thread);  // ← 调试输出

            // 【第2步】在 map 中查找对应的对象
            auto it = m_mapThread.find(thread);  // 找到：{12346, 0x2000}

            // 【第3步】检查是否找到
            if (it != m_mapThread.end()) {  // 找到了 ✅
                printf("[调试] 在 map 中找到了线程对象\n");  // ← 调试输出

                // 【第4步】检查对象指针是否为空
                if (it->second) {  // 0x2000 不是 nullptr ✅
                    printf("[调试] 对象指针有效，进入暂停循环\n");  // ← 调试输出

                    // 【第5步】暂停循环（核心）
                    while (it->second->m_bpaused) {  // 只要 m_bpaused 是 true，就循环

                        // 【第6步】检查是否被 Stop
                        if (it->second->m_thread == 0) {  // 发现 Stop 命令
                            printf("[调试] 检测到 Stop 命令，退出线程\n");  // ← 调试输出
                            pthread_exit(NULL);  // 立即退出
                        }

                        // 【第7步】睡眠 1ms，减少 CPU 占用
                        usleep(1000);
                    }
                    printf("[调试] 退出暂停循环，继续执行\n");  // ← 调试输出
                    // m_bpaused 变成 false 后，跳出循环，继续执行任务
                } else {
                    printf("[调试] 对象指针为空\n");  // ← 调试输出
                }
            } else {
                printf("[调试] 在 map 中没有找到线程对象\n");  // ← 调试输出
            }
        }
        else if (signo == SIGUSR2) {
            printf("[调试] 收到 SIGUSR2，强制退出\n");  // ← 调试输出
            // 处理强制退出信号
            pthread_exit(NULL);
        }
        printf("[调试] Sigaction 返回\n");  // ← 调试输出
    }
    // 成员变量：
    std::function<int()> m_function;  // ← 用 std::function 取代 CFunctionBase*
    pthread_t m_thread;                // ← 线程ID
    bool m_bpaused;                    // ← 暂停标志（true=暂停，false=运行）

    static std::map<pthread_t, CThread*> m_mapThread;  // ← 静态变量 线程映射表
};

