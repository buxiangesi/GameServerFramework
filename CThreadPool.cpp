#include "CThreadPool.h"
#include <stdio.h>

// ============================================
// 构造函数：初始化线程池
// ============================================
CThreadPool::CThreadPool() {
    // 初始化服务器指针
    m_server = nullptr;

    // 获取高精度时间戳（用于生成唯一的Socket文件名）
    timespec tp = { 0, 0 };
    clock_gettime(CLOCK_REALTIME, &tp);

    // 生成唯一的Socket文件名（格式：秒.纳秒.sock）
    char buf[64];
    snprintf(buf, sizeof(buf), "%ld.%ld.sock",
             tp.tv_sec % 100000,    // 取后5位秒数
             tp.tv_nsec % 1000000); // 取后6位纳秒数

    m_path = buf;

    // 延迟1微秒（降低文件名冲突概率）
    usleep(1);
}

// ============================================
// 析构函数：自动关闭线程池
// ============================================
CThreadPool::~CThreadPool() {
    Close();
}

// ============================================
// Start：启动线程池
// 参数 count: 工作线程数量
// 返回值: 0成功，负数失败
// ============================================
int CThreadPool::Start(unsigned count) {
    int ret = 0;

    // 步骤1：防止重复启动
    if (m_server != nullptr) return -1;
    if (m_path.size() == 0) return -2;

    // 步骤2：创建并初始化服务器Socket
    m_server = new CLocalSocket();
    if (m_server == nullptr) return -3;

    ret = m_server->Init(CSockParam(m_path, SOCK_ISSERVER));
    if (ret != 0) return -4;

    // 步骤3：创建Epoll实例
    ret = m_epoll.Create(count);
    if (ret != 0) return -5;

    // 步骤4：注册服务器Socket到Epoll
    ret = m_epoll.Add(*m_server, EpollData((void*)m_server));
    if (ret != 0) return -6;

    // 步骤5：创建工作线程
    m_threads.resize(count);  // 预分配空间
    for (unsigned i = 0; i < count; i++) {
        // 创建线程，执行 TaskDispatch 函数
        m_threads[i] = new CThread(&CThreadPool::TaskDispatch, this);
        if (m_threads[i] == nullptr) return -7;

        ret = m_threads[i]->Start();
        if (ret != 0) return -8;
    }

    return 0;
}

// ============================================
// Close：关闭线程池（优雅关闭）
// ============================================
void CThreadPool::Close() {
    // 步骤1：关闭Epoll（通知线程退出）
    m_epoll.Close();

    // 步骤2：关闭服务器Socket（先置NULL再删除，保护多线程访问）
    if (m_server) {
        CSocketBase* p = m_server;
        m_server = nullptr;  // 立即设置为NULL
        delete p;            // 慢慢删除
    }

    // 步骤3：停止所有工作线程
    for (auto thread : m_threads) {
        if (thread) {
            delete thread;  // 调用析构函数，等待线程退出
        }
    }
    m_threads.clear();  // 清空vector

    // 步骤4：删除Socket文件
    unlink(m_path);
}

// ============================================
// TaskDispatch：任务分发函数（工作线程执行）
// ============================================
int CThreadPool::TaskDispatch() {
    // 主循环：持续监听任务
    while (m_epoll != -1) {
        EPEvents events;
        int ret = 0;

        // 等待事件（阻塞）
        ssize_t esize = m_epoll.WaitEvents(events);

        if (esize > 0) {
            // 遍历所有就绪事件
            for (ssize_t i = 0; i < esize; i++) {

                // 检查是否可读事件
                if (events[i].events & EPOLLIN) {
                    CSocketBase* pClient = nullptr;

                    // 判断事件类型（通过指针区分）
                    // 注意：需要先保存 m_server，避免竞态
                    CSocketBase* server = m_server;

                    if (server && events[i].data.ptr == server) {
                        //──────────────────────────────
                        // 场景1：新连接到达
                        //──────────────────────────────

                        // Accept 接受连接（使用保存的 server）
                        ret = server->Link(&pClient);
                        if (ret != 0) continue;

                        // 注册客户端Socket到Epoll（检查 epoll 是否有效）
                        if (m_epoll != -1) {
                            ret = m_epoll.Add(*pClient, EpollData((void*)pClient));
                            if (ret != 0) {
                                delete pClient;
                                continue;
                            }
                        } else {
                            // epoll 已关闭，放弃此连接
                            delete pClient;
                            continue;
                        }

                    } else {
                        //──────────────────────────────
                        // 场景2：客户端发来任务数据
                        //──────────────────────────────

                        pClient = (CSocketBase*)events[i].data.ptr;

                        if (pClient) {
                            // 接收任务指针
                            std::function<int()>* base = nullptr;
                            Buffer data(sizeof(base));

                            ret = pClient->Recv(data);
                            if (ret <= 0) {
                                // 接收失败或连接断开，删除连接
                                if (m_epoll != -1) {
                                    m_epoll.Del(*pClient);
                                }
                                delete pClient;
                                continue;
                            }

                            // 解析指针
                            memcpy(&base, (char*)data, sizeof(base));

                            if (base != nullptr) {
                                // 执行任务
                                (*base)();

                                // 释放任务对象
                                delete base;
                            }
                            // ✅ 任务完成，保持连接（不删除 pClient）
                            // 客户端可以继续发送下一个任务
                        }
                    }
                }
            }
        }
    }

    return 0;
}
