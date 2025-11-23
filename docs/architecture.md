# GameServerFramework 架构设计文档

## 目录

1. [系统架构](#系统架构)
2. [模块设计](#模块设计)
3. [类图关系](#类图关系)
4. [数据流设计](#数据流设计)
5. [扩展指南](#扩展指南)

---

## 系统架构

### 整体架构

```
┌─────────────────────────────────────────────────────┐
│           GameServerFramework 架构                   │
└─────────────────────────────────────────────────────┘

        应用层（Game Logic）
              ↓
    ┌──────────────────────┐
    │   GameServer 层       │  - 游戏逻辑
    │   - Session 管理      │  - 玩家管理
    │   - 业务逻辑          │  - 协议处理
    └──────────────────────┘
              ↓
    ┌──────────────────────┐
    │   网络层（Network）   │  - Socket 封装
    │   - Socket 模块       │  - Epoll 多路复用
    │   - Epoll 模块        │  - I/O 事件处理
    └──────────────────────┘
              ↓
    ┌──────────────────────┐
    │   通用层（Common）    │  - Buffer 管理
    │   - 工具类            │  - 线程池
    │   - 内存池            │  - 对象池
    └──────────────────────┘
              ↓
    ┌──────────────────────┐
    │   系统层（System）    │  - Linux API
    │   - Socket API        │  - Epoll API
    │   - Thread API        │  - Process API
    └──────────────────────┘
```

---

## 模块设计

### 1. 网络模块（Network）

#### Socket 子模块

**职责：** 封装 Socket API，提供统一的网络通信接口

**类图：**

```
        CSocketBase（抽象基类）
               ↑
      ┌────────┼────────┐
      │        │        │
CLocalSocket CTcpSocket CUdpSocket
```

**关键接口：**

```cpp
class CSocketBase {
public:
    virtual int Init(const CSockParam& param) = 0;
    virtual int Link(CSocketBase** pClient = NULL) = 0;
    virtual int Send(const Buffer& data) = 0;
    virtual int Recv(Buffer& data) = 0;
    virtual int Close() = 0;
};
```

---

#### Epoll 子模块

**职责：** I/O 多路复用，事件驱动

**核心类：**

```cpp
class CEpoll {
public:
    int Create(int size);
    int Add(int fd, const EpollData& data, uint32_t events);
    int Modify(int fd, uint32_t events, const EpollData& data);
    int Del(int fd);
    ssize_t WaitEvents(EPEvents& events, int timeout = -1);
};
```

---

### 2. 服务器模块（Server）

#### GameServer 类（待实现）

**职责：** 游戏服务器主类，管理整个服务器生命周期

**功能：**
- 初始化网络模块
- 管理客户端连接
- 分发业务逻辑
- 定时器管理

**设计草图：**

```cpp
class GameServer {
public:
    int Init(const ServerConfig& config);
    int Start();
    int Stop();

private:
    void OnAccept(int fd);
    void OnRecv(Session* session, const Buffer& data);
    void OnDisconnect(Session* session);

    CEpoll m_epoll;
    CSocketBase* m_listen_socket;
    std::map<int, Session*> m_sessions;
};
```

---

#### Session 类（待实现）

**职责：** 管理单个客户端连接

**功能：**
- 接收/发送数据
- 协议解析
- 会话状态管理

**设计草图：**

```cpp
class Session {
public:
    int Send(const Buffer& data);
    int Recv(Buffer& data);
    void Close();

    int GetFd() const;
    uint64_t GetSessionId() const;

private:
    int m_fd;
    uint64_t m_session_id;
    Buffer m_recv_buffer;
    Buffer m_send_buffer;
};
```

---

### 3. 通用模块（Common）

#### Buffer 类

**职责：** 智能缓冲区管理

**特性：**
- 继承 `std::string`
- 自动类型转换
- RAII 自动管理

```cpp
class Buffer : public std::string {
public:
    Buffer(size_t size);
    operator char*();
    operator const char*() const;
};
```

---

## 类图关系

### Socket 模块类图

```
┌─────────────────────────────────────┐
│         CSockParam                  │  参数封装
│  - sockaddr_in addr_in              │
│  - sockaddr_un addr_un              │
│  - Buffer ip                        │
│  - short port                       │
│  - int attr                         │
└─────────────────────────────────────┘
                ↓ 使用
┌─────────────────────────────────────┐
│         CSocketBase                 │  抽象基类
│  # int m_socket                     │
│  # int m_status                     │
│  # CSockParam m_param               │
│  + virtual Init()                   │
│  + virtual Link()                   │
│  + virtual Send()                   │
│  + virtual Recv()                   │
│  + virtual Close()                  │
└─────────────────────────────────────┘
       ↑              ↑            ↑
       │              │            │
┌──────┴──────┐ ┌────┴──────┐ ┌──┴────────┐
│CLocalSocket │ │CTcpSocket │ │CUdpSocket │
└─────────────┘ └───────────┘ └───────────┘
```

---

### Epoll 模块类图

```
┌─────────────────────────────────────┐
│         EpollData                   │  数据封装
│  - epoll_data_t m_data              │
│  + EpollData(int fd)                │
│  + EpollData(void* ptr)             │
│  + operator epoll_data_t()          │
└─────────────────────────────────────┘
                ↓ 使用
┌─────────────────────────────────────┐
│         CEpoll                      │  Epoll 管理
│  - int m_epoll                      │
│  + Create(int size)                 │
│  + Add(fd, data, events)            │
│  + Modify(fd, events, data)         │
│  + Del(fd)                          │
│  + WaitEvents(events, timeout)      │
└─────────────────────────────────────┘
```

---

## 数据流设计

### 服务器启动流程

```
main()
  │
  ├─> GameServer::Init()
  │     ├─> 加载配置
  │     ├─> 创建监听 Socket
  │     ├─> 创建 Epoll 实例
  │     └─> 添加监听 Socket 到 Epoll
  │
  └─> GameServer::Start()
        └─> 事件循环
              ├─> epoll.WaitEvents()
              ├─> 处理新连接（Accept）
              ├─> 处理数据接收（Recv）
              └─> 处理数据发送（Send）
```

---

### 客户端连接流程

```
客户端连接请求
  │
  ├─> Epoll 检测到 EPOLLIN 事件
  │
  ├─> GameServer::OnAccept()
  │     ├─> server->Link(&client)
  │     ├─> 创建 Session 对象
  │     └─> epoll.Add(client_fd, session)
  │
  └─> Session 加入会话列表
```

---

### 数据接收流程

```
客户端发送数据
  │
  ├─> Epoll 检测到 EPOLLIN 事件
  │
  ├─> GameServer::OnRecv(session)
  │     ├─> session->Recv(buffer)
  │     ├─> 协议解析
  │     └─> 业务逻辑处理
  │
  └─> 回复数据（如需）
        └─> session->Send(response)
```

---

## 扩展指南

### 1. 添加新的 Socket 类型

**步骤：**

1. 继承 `CSocketBase` 基类
2. 实现 5 个纯虚函数
3. 根据协议特性修改实现

**示例：添加 SSL Socket**

```cpp
class CSslSocket : public CSocketBase {
public:
    virtual int Init(const CSockParam& param) override;
    virtual int Link(CSocketBase** pClient = NULL) override;
    virtual int Send(const Buffer& data) override;
    virtual int Recv(Buffer& data) override;
    virtual int Close() override;

private:
    SSL_CTX* m_ssl_ctx;
    SSL* m_ssl;
};
```

---

### 2. 添加协议层

**目的：** 解决粘包/半包问题

**设计方案：**

```cpp
// 协议格式：[4字节长度] + [数据]
class CProtocol {
public:
    int SendMessage(CSocketBase* sock, const Buffer& msg);
    int RecvMessage(CSocketBase* sock, Buffer& msg);

private:
    Buffer m_recv_buffer;  // 接收缓冲区
};
```

---

### 3. 添加线程池

**目的：** 多线程处理业务逻辑

**设计方案：**

```cpp
class ThreadPool {
public:
    void Init(int thread_count);
    void AddTask(std::function<void()> task);
    void Stop();

private:
    std::vector<std::thread> m_threads;
    std::queue<std::function<void()>> m_tasks;
    std::mutex m_mutex;
    std::condition_variable m_cv;
};
```

---

### 4. 添加日志系统

**目的：** 记录运行状态，便于调试

**设计方案：**

```cpp
class Logger {
public:
    static void Debug(const char* fmt, ...);
    static void Info(const char* fmt, ...);
    static void Warn(const char* fmt, ...);
    static void Error(const char* fmt, ...);

private:
    static FILE* m_log_file;
    static int m_log_level;
};
```

---

## 性能优化建议

### 1. 网络层优化

- 使用 `SO_REUSEADDR` 和 `SO_REUSEPORT`
- 设置 `TCP_NODELAY` 禁用 Nagle 算法
- 调整发送/接收缓冲区大小
- 使用非阻塞 I/O

### 2. 内存优化

- 使用内存池减少 `new/delete`
- 使用对象池复用对象
- 减少内存拷贝（使用引用）

### 3. 并发优化

- 多进程 + Epoll（充分利用多核）
- 线程池处理业务逻辑
- 无锁队列减少竞争

---

**更新日期：** 2025-11-23
**版本：** v1.0.0
