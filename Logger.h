#pragma once
#include "Thread.h"
#include "Epoll.h"
#include "Socket.h"
#include <list>
#include <sys/timeb.h>
#include <sys/stat.h>
#include"Socket.h"

// ============================================
// LogInfo类 - 日志信息封装（阶段5实现）
// ============================================
class LogInfo {
public:
    LogInfo() {}
    ~LogInfo() {}

    // 类型转换操作符：将LogInfo转换为Buffer（后续实现）
    operator Buffer() const {
        // TODO: 后续实现
        return Buffer();
    }
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
    static Buffer GetTimeStr();

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



// Close方法（占位，下次详细实现）
inline int CLoggerServer::Close() {
    // TODO: 下次实现
    // 1. 关闭Socket
    // 2. 关闭epoll
    // 3. 停止线程
    // 4. 关闭文件
    return 0;
}

// Trace静态方法（占位，下次详细实现）
inline void CLoggerServer::Trace(const LogInfo& info) {
    // TODO: 下次实现
    // 1. 获取thread_local的Socket
    // 2. 连接到服务器（如果未连接）
    // 3. 发送日志数据
}

// GetTimeStr静态方法（占位，下次详细实现）
inline Buffer CLoggerServer::GetTimeStr() {
    // TODO: 下次实现
    // 1. 获取当前时间
    // 2. 格式化为字符串
    // 3. 返回Buffer
    return Buffer();
}

// ThreadFunc线程函数（占位，下次详细实现）
inline int CLoggerServer::ThreadFunc() {
    // TODO: 下次实现
    // 1. epoll事件循环
    // 2. 处理新连接
    // 3. 接收日志数据
    // 4. 写入文件
    return 0;
}

// WriteLog私有方法（占位，下次详细实现）
inline void CLoggerServer::WriteLog(const Buffer& data) {
    // TODO: 下次实现
    // fwrite(data, ..., m_file);
    // fflush(m_file);
}
