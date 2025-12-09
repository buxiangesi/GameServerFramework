#pragma once
#include "Thread.h"
#include "Epoll.h"
#include "Socket.h"
#include <list>
#include <sys/timeb.h>
#include <sys/stat.h>
#include <sys/timeb.h>
#include <stdarg.h>      // va_list, va_start, va_end（可变参数）
#include <sstream>       // std::stringstream（流式输出）
#include <sys/types.h>   // pid_t
#include <unistd.h>      // getpid()
#include <pthread.h>     // pthread_t, pthread_self()
// ============================================
// LogInfo类 - 日志信息封装（阶段5实现）
// ============================================
enum LogLevel {
    LOG_INFO,      // 0
    LOG_DEBUG,     // 1
    LOG_WARNING,   // 2
    LOG_ERROR,     // 3
    LOG_FATAL      // 4
};
// ==================== 2. LogInfo类声明 ====================
class LogInfo {
public:
    // 构造函数1：printf风格（可变参数）
    // 用法：TRACEI("User: %d", userId)
    LogInfo(
        const char* file, int line, const char* func,
        pid_t pid, pthread_t tid, int level,
        const char* fmt, ...);  // 格式字符串 + 可变参数

    // 构造函数2：流式输出风格
    // 用法：LOGI << "User: " << userId
    LogInfo(
        const char* file, int line, const char* func,
        pid_t pid, pthread_t tid, int level);

    // 构造函数3：内存dump风格
    // 用法：DUMPI(buffer, 256)
    LogInfo(
        const char* file, int line, const char* func,
        pid_t pid, pthread_t tid, int level,
        void* pData, size_t nSize);  // 内存地址 + 字节数

    // 析构函数（流式输出风格会在这里自动发送日志）
    ~LogInfo();

    // 类型转换运算符：让LogInfo能转换为Buffer（给Trace()用）
    operator Buffer() const {
        return m_buf;
    }

    // 流式输出运算符：支持 << 操作（链式调用）
    template<typename T>
    LogInfo& operator<<(const T& data) {
        std::stringstream stream;
        stream << data;           // 转换为字符串
        m_buf += stream.str();    // 追加到缓冲区
        return *this;             // 返回自己（支持链式调用）
    }

private:
    bool bAuto;    // 标志位：false=手动发送（printf/dump）,
    //true = 析构时自动发送（流式输出）
      Buffer m_buf;  // 日志内容缓冲区
};



// ============================================
// CLoggerServer类 - 异步日志服务器
//
// 设计模式：多生产者-单消费者（MPSC）
// 通信方式：Unix Domain Socket
// 并发模型：epoll事件驱动 + 单线程处理
// ============================================
class CLoggerServer
{
public:
    // ========================================
    // 构造函数
    // ========================================
    CLoggerServer() :
       m_thread([this]() { return this->ThreadFunc(); })  // 初始化列表：避免二次构造
        // 为什么用初始化列表？
        // 1. CThread可能没有默认构造函数，必须直接构造
        // 2. 效率更高，避免"默认构造 + 赋值"的开销
        // 3. 传入this指针，让线程函数能访问成员变量
    {
        // 延迟初始化：构造时不创建Socket
        // 原因：Socket需要文件系统路径，目录可能还不存在
        m_server = NULL;

        // 动态生成日志文件名：包含时间戳
        // 格式：./log/2025-01-15 14-30-25 123.log
        // 好处：
        // 1. 每次启动生成新文件，不覆盖历史日志
        // 2. 文件名自带时间，方便定位问题
        // 3. 自动归档（按启动时间分开）
        m_path = "./log/" + GetTimeStr() + ".log";

        // 调试输出：打印日志路径（__FILE__、__LINE__便于定位）
        printf("%s(%d):[%s]path=%s\n", __FILE__, __LINE__, __FUNCTION__,
            (char*)m_path);
    }

    // ========================================
    // 析构函数
    // ========================================
    ~CLoggerServer() {
        // RAII原则：资源获取即初始化
        Close();
    }

public:
    // ========================================
    // 禁止拷贝和赋值
    // ========================================
    // 为什么禁止？
    // 1. m_thread是线程对象，不应该被拷贝（会创建多个线程）
    // 2. m_server是指针，浅拷贝会导致多次delete
    // 3. m_file是文件句柄，拷贝会导致多次fclose
    //
    // C++11方式：= delete（编译期禁止）
    CLoggerServer(const CLoggerServer&) = delete;
    CLoggerServer& operator=(const CLoggerServer&) = delete;

public:
    // ========================================
    // 公共接口（下次实现详细逻辑）
    // ========================================

    // 启动日志服务
    // 功能：
    // 1. 创建log目录
    // 2. 打开日志文件
    // 3. 创建epoll实例
    // 4. 创建Unix Socket服务器
    // 5. 启动日志处理线程
    int Start();

    // 关闭日志服务
    // 功能：
    // 1. 关闭服务器Socket
    // 2. 关闭epoll
    // 3. 停止线程
    // 4. 关闭文件
    int Close();

    // 静态接口：业务线程调用此方法记录日志
    // 特点：
    // 1. static：可以直接类名调用，无需对象
    // 2. thread_local：每个线程独立Socket，无需加锁
    // 3. 异步：发送后立即返回，不等待I/O
    static void Trace(const LogInfo& info);

    // 工具函数：生成时间字符串
    // 格式：2025-01-15 14-30-25 123
    // 用途：
    // 1. 生成日志文件名
    // 2. 日志内容的时间戳
    static Buffer GetTimeStr() {
        Buffer result(128);                    // [1] 预分配缓冲区
        timeb tmb;                             // [2] 定义时间结构体
        ftime(&tmb);                           // [3] 获取当前时间
        // ✅ 线程安全版本
        tm local_tm;  // 栈上分配，线程独立
        localtime_r(&tmb.time, &local_tm);  // POSIX版本
        int nSize = snprintf(result, result.size(),
            "%04d-%02d-%02d %02d-%02d-%02d %03d",
            local_tm.tm_year + 1900,  // ✅ 读取栈上的数据
            local_tm.tm_mon + 1,
            local_tm.tm_mday,
            local_tm.tm_hour,
            local_tm.tm_min,
            local_tm.tm_sec,
            tmb.millitm
        );
        //int result = snprintf(buffer, sizeof(buffer), "Hello %s, age=%d", "张三", 25);
        ////                     ↑        ↑              ↑                   ↑      ↑
        ////                   目标缓冲   缓冲区大小    格式化字符串        参数1  参数2
        result.resize(nSize);                  // [6] 调整实际大小
        return result;                         // [7] 返回
    }

private:
    // ========================================
    // 线程函数（下次实现详细逻辑）
    // ========================================
    // 在独立线程中运行，负责：
    // 1. epoll监听客户端连接和数据
    // 2. 接收日志数据
    // 3. 写入磁盘文件
    int ThreadFunc();

    // 写日志到文件
    // 注意：只在日志线程中调用，串行执行，无需加锁
    void WriteLog(const Buffer& data);

private:
    // ========================================
    // 成员变量
    // ========================================

    // 日志处理线程
    // 为什么是对象不是指针？
    // - 生命周期与CLoggerServer一致，不需要动态创建
    // - 简化管理，析构时自动销毁
    CThread m_thread;

    // epoll实例：I/O多路复用
    // 作用：单线程同时监听多个客户端连接
    // 优势：比select/poll性能更好（O(1)复杂度）
    CEpoll m_epoll;

    // 服务器Socket：接受客户端连接
    // 为什么是指针？
    // - 需要延迟初始化（构造时目录可能不存在）
    // - Start()时才创建，Close()时delete
    CSocketBase* m_server;

    // 日志文件路径
    // 类型：Buffer（自动管理内存的字符串类）
    Buffer m_path;

    // 文件句柄
    // 为什么用FILE*而不是ofstream？
    // 1. 性能更好（减少C++流的封装开销）
    // 2. 精确控制：可用fflush()强制刷盘
    // 3. 简单直接：fwrite比流操作快
    FILE* m_file;
};

// ============================================
// 实现部分（简化的占位实现）
// ============================================



  // Close方法：释放所有资源
inline int CLoggerServer::Close() {
    // ========================================
    // 步骤1：关闭服务器Socket
    // ========================================
    // 为什么第一个关闭？
    // - 停止接受新的客户端连接
    // - 防止Start()失败时重复删除
    if (m_server != NULL) {
        // ⭐ 安全删除技巧：先保存指针，再置空，最后delete
        CSocketBase* p = m_server;  // 保存指针
        m_server = NULL;            // 立即置空（防止重复释放）
        delete p;                   // 删除对象

        // 为什么这样写？
        // 如果直接 delete m_server; m_server = NULL;
        // 在多线程环境下，delete和赋值之间可能被打断
        // 导致m_server指向已释放的内存（野指针）
    }

    // ========================================
    // 步骤2：关闭epoll
    // ========================================
    // 为什么第二个关闭？
    // - Socket已关闭，不会再有新事件
    // - 停止事件监听循环
    m_epoll.Close();

    // ========================================
    // 步骤3：停止日志线程
    // ========================================
    // 为什么第三个关闭？
    // - epoll已关闭，ThreadFunc的WaitEvents会返回
    // - 线程会自然退出循环
    // - Stop()会等待线程完全退出
    m_thread.Stop();

    // ========================================
    // 步骤4：关闭日志文件
    // ========================================
    // 为什么最后关闭？
    // - 线程已停止，不会再有写入操作
    // - 安全关闭文件，避免数据丢失
    if (m_file != NULL) {
        fclose(m_file);   // 关闭文件（自动fflush）
        m_file = NULL;    // 置空，防止重复关闭
    }

    return 0;
}

// ThreadFunc线程函数（占位，下次详细实现）
inline int CLoggerServer::ThreadFunc() {
    EPEvents events;
    std::map<int, CSocketBase*> mapClients;

    // 主事件循环：三重保险退出条件
    while (m_thread.isValid() && (m_epoll != -1) && (m_server != NULL)) {
        // 等待事件（1ms超时：平衡响应速度和CPU占用）
        ssize_t ret = m_epoll.WaitEvents(events, 1);
        if (ret < 0) break;  // epoll出错

        if (ret > 0) {
            ssize_t i = 0;
            for (; i < ret; i++) {
                // 检查错误事件
                if (events[i].events & EPOLLERR) {
                    break;  // 快速失败
                }
                // 处理可读事件
                else if (events[i].events & EPOLLIN) {
                    // 判断：新连接 vs 数据到达
                    if (events[i].data.ptr == m_server) {
                        // ========== 新连接 ==========
                        CSocketBase* pClient = NULL;
                        int r = m_server->Link(&pClient);
                        if (r < 0) continue;

                        // 添加到epoll（存储pClient指针到data.ptr）
                        r = m_epoll.Add(*pClient, EpollData((void*)pClient),
                            EPOLLIN | EPOLLERR);
                        if (r < 0) {
                            delete pClient;
                            continue;
                        }

                        // 存入客户端容器（检查fd复用）
                        int fd = (int)(*pClient);  // ✅ 修复：显式转换
                        auto it = mapClients.find(fd);
                        if (it != mapClients.end() && it->second != NULL) {
                            delete it->second;  // 删除旧客户端
                        }
                        mapClients[fd] = pClient;
                    }
                    else {
                        // ========== 数据到达 ==========
                        CSocketBase* pClient = (CSocketBase*)events[i].data.ptr;
                        if (pClient != NULL) {
                            Buffer data(1024 * 1024);  // 1MB缓冲区
                            int r = pClient->Recv(data);

                            if (r <= 0) {
                                // 连接断开
                                int fd = (int)(*pClient);  // ✅ 修复：显式转换
                                delete pClient;
                                mapClients[fd] = NULL;  //标记删除，避免遍历中erase
                            }
                            else {
                                // 写入日志
                                WriteLog(data);
                            }
                        }
                    }
                }
            }

            // 检测循环提前退出（遇到EPOLLERR）
            if (i != ret) {
                break;
            }
        }
    }

    // 退出清理：删除所有客户端
    for (auto it = mapClients.begin(); it != mapClients.end(); it++) {
        if (it->second) {
            delete it->second;
        }
    }
    mapClients.clear();

    return 0;
}
// ==================== 3. 宏定义（用户接口）====================
#ifndef TRACE

// -------- TRACE系列：printf风格 --------
// 用法：TRACEI("User %d login", userId);
#define TRACEI(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, \
    __FUNCTION__, getpid(), pthread_self(), LOG_INFO, __VA_ARGS__))
#define TRACED(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, \
    __FUNCTION__, getpid(), pthread_self(), LOG_DEBUG, __VA_ARGS__))
#define TRACEW(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, \
    __FUNCTION__, getpid(), pthread_self(), LOG_WARNING, __VA_ARGS__))
#define TRACEE(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, \
    __FUNCTION__, getpid(), pthread_self(), LOG_ERROR, __VA_ARGS__))
#define TRACEF(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, \
    __FUNCTION__, getpid(), pthread_self(), LOG_FATAL, __VA_ARGS__))

    // -------- LOG系列：流式输出风格 --------
    // 用法：LOGI << "User " << userId << " login";
#define LOGI LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), \
    LOG_INFO)
#define LOGD LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), \
    LOG_DEBUG)
#define LOGW LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), \
    LOG_WARNING)
#define LOGE LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), \
    LOG_ERROR)
#define LOGF LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), \
    LOG_FATAL)

    // -------- DUMP系列：内存dump风格（已修复bug）--------
    // 用法：DUMPI(buffer, 256);
    // 输出：十六进制 + ASCII可视化
#define DUMPI(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, \
    __FUNCTION__, getpid(), pthread_self(), LOG_INFO, data, size))
#define DUMPD(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, \
    __FUNCTION__, getpid(), pthread_self(), LOG_DEBUG, data, size))
#define DUMPW(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, \
    __FUNCTION__, getpid(), pthread_self(), LOG_WARNING, data, size))
#define DUMPE(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, \
    __FUNCTION__, getpid(), pthread_self(), LOG_ERROR, data, size))
#define DUMPF(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, \
    __FUNCTION__, getpid(), pthread_self(), LOG_FATAL, data, size))

#endif


