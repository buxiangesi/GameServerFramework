#pragma once
#include "Epoll.h"
#include "Thread.h"
#include "Socket.h"
#include <vector>
#include <functional>  // std::function
#include <tuple>       // std::tuple
#include <utility>     // std::forward
#include <time.h>

class CThreadPool
{
public:
    CThreadPool();
    ~CThreadPool();

    // 禁止拷贝和赋值（线程池管理独占资源）
    CThreadPool(const CThreadPool&) = delete;
    CThreadPool& operator=(const CThreadPool&) = delete;

public:
    // 启动线程池
    // 参数 count: 工作线程数量（通常设置为CPU核心数）
    // 返回值: 0成功，负数失败
    int Start(unsigned count);

    // 关闭线程池（优雅关闭，等待线程退出）
    void Close();

    // 添加任务到线程池（模板函数，支持任意函数和参数）
    // 参数 func: 函数指针、成员函数指针、lambda等
    // 参数 args: 函数参数（可变参数）
    // 返回值: 0成功，负数失败
    template<typename _FUNCTION_, typename... _ARGS_>
    int AddTask(_FUNCTION_ func, _ARGS_... args);

private:
    // 任务分发函数（工作线程执行）
    int TaskDispatch();

private:
    CEpoll m_epoll;                    // Epoll实例（监听任务到达）
    std::vector<CThread*> m_threads;   // 工作线程数组
    CSocketBase* m_server;             // 服务器Socket（接收任务连接）
    Buffer m_path;                     // Socket文件路径（Unix Domain Socket）
};

// ============================================
// AddTask 模板函数实现（必须放在头文件）
// ============================================
template<typename _FUNCTION_, typename... _ARGS_>
int CThreadPool::AddTask(_FUNCTION_ func, _ARGS_... args) {
    // 每个调用线程独立的客户端Socket（长连接，无需加锁）
    static thread_local CLocalSocket client;
    int ret = 0;

    // 首次调用时建立连接
    if (client == -1) {
        ret = client.Init(CSockParam(m_path, 0));  // 客户端模式
        if (ret != 0) return -1;

        ret = client.Link();  // 连接到服务器
        if (ret != 0) return -2;
    }

    // 封装任务：使用lambda捕获函数和参数
    std::function<int()>* base = new std::function<int()>(
        [func, args...]() -> int {
            func(args...);  // 调用函数
            return 0;
        }
    );

    if (base == NULL) return -3;

    // 准备数据：只发送指针（8字节）
    Buffer data(sizeof(base));
    memcpy(data, &base, sizeof(base));

    // 通过Socket发送指针
    ret = client.Send(data);
    if (ret <= 0) {  // Send返回发送字节数，<=0表示失败
        delete base;  // 发送失败，释放任务对象
        return -4;
    }

    // ✅ thread_local client 保持连接（长连接），可重复使用
    return 0;
}
