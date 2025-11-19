#pragma once

// ============================================
// 必要的头文件
// ============================================
#include <unistd.h>       // close()
#include <sys/epoll.h>    // epoll_create, epoll_ctl, epoll_wait
#include <vector>         // std::vector
#include <errno.h>        // errno, EINTR, EAGAIN
#include <sys/signal.h>   // signal 相关
#include <memory.h>       // memset, memcpy
#include <cstdio>

// 事件数组的默认大小
#define EVENT_SIZE 128

// ============================================
// EpollData 类
// 作用：封装 epoll_data_t 联合体，提供类型安全
// ============================================

class EpollData
{
public:
    // ────────────────────────────────────────
    // 构造函数 - 创建对象时调用
    // ────────────────────────────────────────

   
    //  默认构造函数
    //  作用：初始化为 0，避免随机值
    // 
    //  为什么用 u64？
    //    - u64 是联合体中最大的成员（8字节）
    //    - 设置 u64 = 0 会清空整个联合体的内存
    //
    EpollData() {
        m_data.u64 = 0;
    }

    
    // * void* 构造函数
    // * 使用场景：存储自定义对象的指针
    // *
    // * 例如：
    // *   Connection* conn = new Connection();
    // *   EpollData data(conn);  // 存储指针
    // *
    // * 注意：没有 explicit，允许隐式转换
    // *   EpollData data = (void*)ptr;  // 允许
    // */
    EpollData(void* ptr) {
        m_data.ptr = ptr;
    }

   
    // * int 构造函数（文件描述符）
    // * 使用场景：存储 socket fd、文件 fd 等
    // *
    // * 例如：
    // *   int sock_fd = socket(...);
    // *   EpollData data(sock_fd);  // 存储 fd
    // *
    // * explicit 关键字：
    // *   - 禁止隐式转换
    // *   - EpollData data = 5;      // ❌ 编译错误
    // *   - EpollData data(5);       // ✅ 正确
    // *
    // * 为什么用 explicit？
    // *   - int 可能是 fd，也可能是普通数字
    // *   - 要求显式声明，避免歧义
    // */
    explicit EpollData(int fd) {
        m_data.fd = fd;
    }

    //  uint32_t 构造函数
    //  使用场景：存储 32 位整数（如连接 ID、索引等）
    // 
    //  例如：
    //   uint32_t conn_id = 12345;
    //    EpollData data(conn_id);
    // /
    explicit EpollData(uint32_t u32) {
        m_data.u32 = u32;
    }


     //* uint64_t 构造函数
     //* 使用场景：存储 64 位整数（如时间戳、大ID等）
     //*
     //* 例如：
     //*   uint64_t timestamp = time(NULL);
     //*   EpollData data(timestamp);
    
    explicit EpollData(uint64_t u64) {
        m_data.u64 = u64;
    }


     //* 拷贝构造函数
     //* 作用：用另一个 EpollData 对象创建新对象
     //*
     //* 例如：
     //*   EpollData data1(5);
     //*   EpollData data2(data1);  // 拷贝构造
     //*
     //* 为什么拷贝 u64？
     //*   - u64 占据整个联合体（8字节）
     //*   - 拷贝 u64 = 拷贝整个联合体

    EpollData(const EpollData& data) {
        m_data.u64 = data.m_data.u64;
    }

public:
    // ────────────────────────────────────────
    // 赋值运算符 - 修改已存在的对象
    // ────────────────────────────────────────

    /**
     * 拷贝赋值运算符
     * 作用：用另一个对象的值修改当前对象
     *
     * 例如：
     *   EpollData data1(5);
     *   EpollData data2;
     *   data2 = data1;  // 调用拷贝赋值
     *
     * 自赋值检查：
     *   if (this != &data)  // 防止 a = a
     *
     * 为什么需要检查？
     *   - 自赋值是浪费（不需要操作）
     *   - 某些复杂类型自赋值可能导致错误
     *
     * 返回 *this：
     *   - 支持链式赋值：a = b = c
     */
    EpollData& operator=(const EpollData& data) {
        if (this != &data)
            m_data.u64 = data.m_data.u64;
        return *this;
    }

    /**
     * void* 赋值运算符
     * 使用场景：修改为指针值
     *
     * 例如：
     *   EpollData data;
     *   data = (void*)ptr;  // 修改为指针
     */
    EpollData& operator=(void* data) {
        m_data.ptr = data;
        return *this;
    }

    /**
     * int 赋值运算符
     * 使用场景：修改为文件描述符
     *
     * 例如：
     *   EpollData data;
     *   data = socket_fd;  // 修改为 fd
     */
    EpollData& operator=(int data) {
        m_data.fd = data;
        return *this;
    }

    /**
     * uint32_t 赋值运算符
     * 使用场景：修改为 32 位整数
     */
    EpollData& operator=(uint32_t data) {
        m_data.u32 = data;
        return *this;
    }

    /**
     * uint64_t 赋值运算符
     * 使用场景：修改为 64 位整数
     */
    EpollData& operator=(uint64_t data) {
        m_data.u64 = data;
        return *this;
    }

    // ────────────────────────────────────────
    // 类型转换运算符 - 转换为其他类型
    // ────────────────────────────────────────

    /**
     * 转换为 epoll_data_t（非 const 版本）
     * 作用：让 EpollData 可以直接当作 epoll_data_t 使用
     *
     * 使用场景：
     *   EpollData data(5);
     *   epoll_event ev;
     *   ev.data = data;  // 自动调用此转换运算符
     *
     * 为什么需要两个版本（const 和非 const）？
     *   - 非 const 对象调用非 const 版本
     *   - const 对象调用 const 版本
     */
    operator epoll_data_t() {
        return m_data;
    }

    /**
     * 转换为 epoll_data_t（const 版本）
     */
    operator epoll_data_t() const {
        return m_data;
    }

    /**
     * 转换为 epoll_data_t*（非 const 版本）
     * 作用：返回指向内部数据的指针
     *
     * 使用场景：
     *   EpollData data;
     *   epoll_data_t* ptr = data;  // 获取指针
     */
    operator epoll_data_t* () {
        return &m_data;
    }

    /**
     * 转换为 const epoll_data_t*（const 版本）
     */
    operator const epoll_data_t* () const {
        return &m_data;
    }

private:
    // ────────────────────────────────────────
    // 私有成员变量
    // ────────────────────────────────────────

    /**
     * 实际存储数据的联合体
     *
     * epoll_data_t 的定义（来自 <sys/epoll.h>）：
     *   union epoll_data_t {
     *       void*    ptr;    // 指针
     *       int      fd;     // 文件描述符
     *       uint32_t u32;    // 32位整数
     *       uint64_t u64;    // 64位整数
     *   };
     *
     * 注意：4个成员共享同一块内存，同一时刻只能用一个
     */
    epoll_data_t m_data;
};


// ============================================
// 类型别名 - 事件容器
// ============================================

/**
 * EPEvents = std::vector<epoll_event>
 *
 * 为什么使用 vector？
 *   1. 动态扩容：无需预先知道事件数量
 *   2. 自动管理内存：无需手动 new/delete
 *   3. 标准容器接口：size(), data(), resize() 等
 *
 * 使用示例：
 *   EPEvents events;
 *   int n = epoll.WaitEvents(events);
 *   for (int i = 0; i < n; i++) {
 *       epoll_event& ev = events[i];
 *       // 处理事件...
 *   }
 */
using EPEvents = std::vector<epoll_event>;


// ============================================
// 使用示例（注释中）
// ============================================

/*
// 示例1：存储文件描述符
int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
EpollData data1(sock_fd);  // 调用 EpollData(int)

// 示例2：存储自定义对象指针
struct Connection {
    int fd;
    char buffer[1024];
};
Connection* conn = new Connection();
EpollData data2(conn);  // 调用 EpollData(void*)

// 示例3：存储连接ID
uint32_t conn_id = 12345;
EpollData data3(conn_id);  // 调用 EpollData(uint32_t)

// 示例4：修改已存在的对象
EpollData data4;
data4 = sock_fd;  // 调用 operator=(int)
data4 = conn;     // 调用 operator=(void*)

// 示例5：与原生 API 配合使用
epoll_event ev;
ev.events = EPOLLIN;
ev.data = data1;  // 自动调用 operator epoll_data_t()
*/
class CEpoll
{
public:
    // 构造和析构
    CEpoll() { 
        m_epoll = -1;  // -1 表示"未初始化"
    }
    ~CEpoll() { 
        Close();  // 自动清理资源
    }

    // 禁止拷贝
    CEpoll(const CEpoll&) = delete;
    CEpoll& operator=(const CEpoll&) = delete;

    // 类型转换
    operator int() const { return m_epoll; }

    // 核心功能
    int Create(unsigned count) { 
        if (m_epoll != -1) return -1;  // 已经创建过
        m_epoll = epoll_create(count);
        if (m_epoll == -1) return -2;  // 系统调用失败
        return 0;  // 成功
    }
    ssize_t WaitEvents(EPEvents& events, int timeout = 10) { 
        // 1. 检查是否已创建
        if (m_epoll == -1) return -1;

        // 2. 创建临时数组（固定128个）
        EPEvents evs(EVENT_SIZE);

        // 3. 调用系统函数等待
        int ret = epoll_wait(m_epoll, evs.data(), (int)evs.size(), timeout);
        /*
        *   int ret = epoll_wait(m_epoll,      // 参数1: 哪个epoll
                       evs.data(),   // 参数2: 结果存哪里
                       evs.size(),   // 参数3: 最多几个 (128)
                       10);          // 参数4: 超时10ms
                          数组指针      最大容量
        * 
        * 
        */

        // 4. 处理错误
        if (ret == -1) {
            if ((errno == EINTR) || (errno == EAGAIN)) {
                return 0;  // 被中断，当作无事件
            }
            return -2;  // 真的出错
        }

        // 5. 调整用户数组大小
        if (ret > (int)events.size()) {
            events.resize(ret);
        }

        // 6. 拷贝到用户数组
        memcpy(events.data(), evs.data(), sizeof(epoll_event) * ret);

        return ret;
    }
    int Add(int fd, const EpollData& data ,
        uint32_t events = EPOLLIN) {
        // 1. 检查 epoll 是否已创建
        if (m_epoll == -1) return -1;

        // 2. 构造事件结构体
        epoll_event ev = { events, data };
        //               ^^^^^^  ^^^^
        //               监听什么 存什么数据

        // 3. 添加到 epoll
        int ret = epoll_ctl(m_epoll, EPOLL_CTL_ADD, fd, &ev);
        //                  ^^^^^^^  ^^^^^^^^^^^^^  ^^  ^^^
        //                  哪个epoll  操作类型      目标fd 事件配置

        // 4. 检查结果
        if (ret == -1) return -2;

        return 0;
    }
    int Modify(int fd, uint32_t events,
        const EpollData& data = EpollData((void*)0)) {
        // 1. 检查 epoll 是否已创建
        if (m_epoll == -1) return -1;

        // 2. 构造新的事件结构体
        epoll_event ev = { events, data };

        // 3. 修改配置
        int ret = epoll_ctl(m_epoll, EPOLL_CTL_MOD, fd, &ev);
        //                           ^^^^^^^^^^^^^
        //                           操作类型：MOD（修改）

      // 4. 检查结果
        if (ret == -1) return -2;

        return 0;
    }
    int Del(int fd) { 
        // 1. 检查 epoll 是否已创建
        if (m_epoll == -1) return -1;

        // 2. 删除监听
        int ret = epoll_ctl(m_epoll, EPOLL_CTL_DEL, fd, NULL);
        //                           ^^^^^^^^^^^^^  ^^  ^^^^
        //                           操作类型：删除  目标fd NULL

        // 3. 检查结果
        if (ret == -1) return -2;

        return 0;
    }
    void Close() { 
        if (m_epoll != -1) {
            int fd = m_epoll;
            m_epoll = -1;  // 先标记，即使后面出错，也是安全的
            close(fd);     // 后关闭
        }
    }

private:
    int m_epoll;
};