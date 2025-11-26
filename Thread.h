#pragma once
#include <pthread.h>     // pthread_create, pthread_join 等
#include <signal.h>      // sigaction, SIGUSR1, SIGUSR2
#include <unistd.h>      // usleep
#include <functional>    // std::function
#include <map>           // std::map（管理线程）
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
        // 用 lambda 捕获参数，创建 std::function
        m_function = [f = std::forward<F>(func),
            ... a = std::forward<Args>(args)]() mutable -> int {
            return f(std::forward<Args>(a)...);
            };
    }

    // 析构函数
    ~CThread() {
        // 线程结束时的清理工作（后面实现）
    }

    // 禁用拷贝构造和拷贝赋值
    CThread(const CThread&) = delete;
    CThread& operator=(const CThread&) = delete;

private:
    // 成员变量：
    std::function<int()> m_function;  // ← 用 std::function 取代 CFunctionBase*
    pthread_t m_thread;                // ← 线程ID
    bool m_bpaused;                    // ← 暂停标志（true=暂停，false=运行）

    static std::map<pthread_t, CThread*> m_mapThread;  // ← 静态变量：线程映射表
};

