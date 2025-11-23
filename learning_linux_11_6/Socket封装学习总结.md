# Socket 封装学习总结

## 项目概述

本项目完成了对 Linux Socket API 的 C++ 封装，实现了三种不同协议的统一接口：
- **CLocalSocket**：Unix 域 socket（进程间通信）
- **CTcpSocket**：TCP/IP socket（网络通信）
- **CUdpSocket**：UDP socket（无连接通信)

通过抽象基类 `CSocketBase` 提供统一接口，实现了：
- 类型安全的参数封装
- 状态机管理
- RAII 资源管理
- 多态接口设计

---

## 一、核心概念

### 1.1 什么是 Socket？

**Socket 是进程间通信的端点**，可以理解为"网络编程的接口"。

**三种常用 Socket 类型：**

| 类型 | 地址族 | 传输方式 | 使用场景 |
|------|--------|---------|---------|
| **Unix 域** | AF_UNIX | SOCK_STREAM | 同一台机器进程间通信 |
| **TCP** | AF_INET | SOCK_STREAM | 网络可靠传输（HTTP、FTP） |
| **UDP** | AF_INET | SOCK_DGRAM | 网络快速传输（DNS、视频流） |

**流程对比：**
```cpp
// TCP 服务器（有连接）
socket() → bind() → listen() → accept() → send()/recv() → close()

// UDP 服务器（无连接）
socket() → bind() → sendto()/recvfrom() → close()

// Unix 域（类似 TCP）
socket(AF_UNIX) → bind("/path") → listen() → accept() → send()/recv()
```

---

### 1.2 为什么要封装？

**原生 API 的问题：**
```cpp
// ❌ 原生方式：重复代码多、易出错
int fd = socket(AF_INET, SOCK_STREAM, 0);
struct sockaddr_in addr;
addr.sin_family = AF_INET;
addr.sin_port = htons(8080);
addr.sin_addr.s_addr = inet_addr("0.0.0.0");
bind(fd, (sockaddr*)&addr, sizeof(addr));
listen(fd, 10);
// ... 每次都要写一遍

// ✅ 封装方式：简洁、类型安全
CTcpSocket server;
server.Init(CSockParam("0.0.0.0", 8080, SOCK_ISSERVER));
server.Link(&client);
```

**封装优势：**
1. **统一接口**：TCP/UDP/Unix 域用同一套 API
2. **类型安全**：编译期检查，减少错误
3. **资源管理**：RAII 自动释放资源
4. **状态控制**：防止错误的调用顺序

---

## 二、类层次结构（重点）

### 2.1 整体架构

```
┌─────────────────────────────────────────┐
│  CSocketBase（抽象基类）                 │
│  ├─ 纯虚函数（5个）                      │
│  │   ├─ Init()   : 初始化                │
│  │   ├─ Link()   : 建立连接              │
│  │   ├─ Send()   : 发送数据              │
│  │   ├─ Recv()   : 接收数据              │
│  │   └─ Close()  : 关闭连接              │
│  ├─ m_socket : int（文件描述符）         │
│  └─ m_status : int（状态机）             │
└─────────────────────────────────────────┘
           ↑              ↑            ↑
           │              │            │
    ┌──────┘              │            └─────┐
    │                     │                  │
┌───┴────────┐   ┌────────┴───────┐   ┌─────┴──────┐
│CLocalSocket│   │  CTcpSocket    │   │ CUdpSocket │
│ Unix域     │   │  TCP/IP        │   │  UDP       │
│ AF_UNIX    │   │  AF_INET       │   │  AF_INET   │
│SOCK_STREAM │   │  SOCK_STREAM   │   │ SOCK_DGRAM │
└────────────┘   └────────────────┘   └────────────┘
```

---

### 2.2 辅助类

#### Buffer 类（智能字符串）
```cpp
class Buffer : public std::string {
public:
    Buffer(size_t size) { resize(size); }
    operator char*() { return (char*)c_str(); }  // 自动转换
};
```

**作用：**
- 继承 `std::string`，自动管理内存
- 提供 3 个转换运算符，兼容 C API
- 可以直接用于 `send()`/`recv()`

---

#### CSockParam 类（参数封装）
```cpp
class CSockParam {
    sockaddr_in addr_in;   // TCP/UDP 地址
    sockaddr_un addr_un;   // Unix 域地址
    Buffer ip;             // IP 或路径
    short port;            // 端口
    int attr;              // 属性标志

    sockaddr* addrin() const;  // 返回 TCP/UDP 地址指针
    sockaddr* addrun() const;  // 返回 Unix 域地址指针
};
```

**三个构造函数：**
```cpp
CSockParam();  // 默认构造
CSockParam("192.168.1.1", 8080, SOCK_ISSERVER);  // TCP/UDP
CSockParam("/tmp/server.sock", SOCK_ISSERVER);   // Unix 域
```

---

#### SockAttr 枚举（位标志）
```cpp
enum SockAttr {
    SOCK_ISSERVER = 1,  // 0001：服务器
    SOCK_ISBLOCK  = 2,  // 0010：阻塞模式
};
```

**位运算判断：**
```cpp
// 设置多个标志
int attr = SOCK_ISSERVER | SOCK_ISBLOCK;  // 0011

// 判断是否是服务器
if (attr & SOCK_ISSERVER) {  // 0011 & 0001 = 0001 (真)
    // 服务器逻辑
}
```

---

## 三、状态机设计

### 3.1 四种状态

```
   0             1              2              3
未初始化 ──→ 已初始化 ──→ 已连接 ──→ 已关闭
           Init()        Link()      Close()
```

| 状态 | 值 | 允许调用的函数 |
|------|----|--------------|
| **未初始化** | 0 | `Init()` |
| **已初始化** | 1 | `Link()` |
| **已连接** | 2 | `Send()`, `Recv()`, `Close()` |
| **已关闭** | 3 | 无（对象即将销毁） |

---

### 3.2 服务器 vs 客户端状态

**关键区别：**
- **服务器**：`Link()` 后状态**不变**（还是 1），因为可以继续 accept
- **客户端**：`Link()` 后状态**变为 2**（已连接）

```cpp
// 服务器
server->Init(...);    // m_status = 1
server->Link(&c1);    // m_status = 1（不变！）
server->Link(&c2);    // m_status = 1（可以继续）

// 客户端
client->Init(...);    // m_status = 1
client->Link();       // m_status = 2（已连接）
client->Send(...);    // OK
```

---

## 四、核心函数实现

### 4.1 Init() 函数（7步流程）

```cpp
int CLocalSocket::Init(const CSockParam& param) {
    // 第1步：状态检查
    if (m_status != 0) return -1;

    // 第2步：保存参数
    m_param = param;

    // 第3步：选择类型
    int type = SOCK_STREAM;

    // 第4步：创建 socket
    m_socket = socket(AF_UNIX, type, 0);
    if (m_socket == -1) return -2;

    // 第5步：服务器绑定+监听
    if (param.attr & SOCK_ISSERVER) {
        unlink(param.ip);  // 删除旧文件（仅 Unix 域需要）
        bind(m_socket, param.addrun(), sizeof(sockaddr_un));
        listen(m_socket, 10);
    }

    // 第6步：设置非阻塞
    if (param.attr & SOCK_ISBLOCK) {
        int flags = fcntl(m_socket, F_GETFL, 0);
        fcntl(m_socket, F_SETFL, flags | O_NONBLOCK);
    }

    // 第7步：更新状态
    m_status = 1;
    return 0;
}
```

**关键点：**
- **unlink()** 必须在 bind() 前调用（Unix 域特有）
- **客户端不需要 bind()**，直接创建 socket 即可
- **fcntl()** 用于设置非阻塞模式

---

### 4.2 Link() 函数（双重指针设计）

#### 为什么使用双重指针？

**问题：服务器需要"返回"客户端对象**
```cpp
// accept() 返回新的 fd
int client_fd = accept(server_fd, ...);

// 需要把 fd 封装成对象，并返回给调用者
// 但 Link() 的返回值已经用来表示成功/失败了
```

**解决方案：双重指针参数**
```cpp
int Link(CSocketBase** pClient = NULL);
//       ↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑
//       双重指针：用于"返回"对象

// 服务器调用
CLocalSocket* client = NULL;
server->Link(&client);  // 传入指针的地址
// client 现在指向新创建的客户端对象

// 客户端调用
client->Link();  // 不需要参数
```

---

#### 双重指针原理

```
调用前：
client = NULL

调用 server->Link(&client)：
函数内 pClient = &client（client 变量的地址）

创建新对象：
new_client = new CLocalSocket(fd);

修改外部指针：
*pClient = new_client;
   ↓
修改 client 变量的值
   ↓
client = new_client（指向新对象）
```

**类比：**
```cpp
// 修改 int 变量 → 传 int*
void change_int(int* p) { *p = 100; }
int a = 10;
change_int(&a);  // a 变成 100

// 修改指针变量 → 传指针*（即双重指针**）
void change_ptr(CLocalSocket** pp) { *pp = new CLocalSocket(); }
CLocalSocket* client = NULL;
change_ptr(&client);  // client 指向新对象
```

---

#### Link() 完整实现

```cpp
int CLocalSocket::Link(CSocketBase** pClient) {
    if (m_status != 1) return -1;

    if (m_param.attr & SOCK_ISSERVER) {
        // ========== 服务器逻辑 ==========
        if (pClient == NULL) return -2;  // 必须传参数

        // accept 等待连接
        sockaddr_un client_addr;
        socklen_t len = sizeof(client_addr);
        int client_fd = accept(m_socket, (sockaddr*)&client_addr, &len);
        if (client_fd == -1) return -3;

        // 创建客户端对象
        CLocalSocket* new_client = new CLocalSocket(client_fd);
        new_client->m_status = 2;  // 已连接

        // 通过双重指针返回
        *pClient = new_client;

        // 服务器状态不变（还是 1）
        return 0;

    } else {
        // ========== 客户端逻辑 ==========
        if (connect(m_socket, m_param.addrun(), sizeof(sockaddr_un)) == -1) {
            return -4;
        }

        m_status = 2;  // 已连接
        return 0;
    }
}
```

---

### 4.3 Send() 和 Recv() 函数

**Send()：**
```cpp
int Send(const Buffer& data) {
    if (m_status != 2) return -1;
    return send(m_socket, data, data.size(), 0);
    //           ↑↑↑↑↑↑  ↑↑↑↑
    //           fd      Buffer 自动转换为 char*
}
```

**Recv()：**
```cpp
int Recv(Buffer& data) {
    if (m_status != 2) return -1;
    return recv(m_socket, data, data.size(), 0);
    // 返回值：>0（字节数）、0（连接关闭）、-1（失败）
}
```

**当前实现的局限性：**
- ❌ 未处理**粘包**问题（多条消息粘在一起）
- ❌ 未处理**半包**问题（消息未收全）
- ✅ 只是基础封装，后续需要**协议层**解决

---

### 4.4 Close() 函数（防重复关闭）

```cpp
int Close() {
    if (m_status == 0 || m_status == 3) return -1;

    // ❌ 错误写法
    // close(m_socket);
    // m_socket = -1;

    // ✅ 正确写法：先设 -1，再关闭
    if (m_socket != -1) {
        int fd = m_socket;
        m_socket = -1;  // 先标记为无效
        close(fd);      // 再关闭
    }

    m_status = 3;
    return 0;
}
```

**为什么先设 -1？**
- 防止多线程/信号中断导致重复关闭
- 即使 `close()` 失败，`m_socket` 也是安全的 -1

---

## 五、三种 Socket 实现对比

### 5.1 CLocalSocket → CTcpSocket（只需改 3 处）

| 修改位置 | CLocalSocket | CTcpSocket |
|---------|-------------|-----------|
| **1. 地址族** | `socket(AF_UNIX, ...)` | `socket(AF_INET, ...)` |
| **2. unlink** | `unlink(param.ip)` | 删除此行 |
| **3. 地址类型** | `addrun()`, `sockaddr_un` | `addrin()`, `sockaddr_in` |

---

### 5.2 CTcpSocket → CUdpSocket（特殊修改）

| 特性 | TCP | UDP |
|------|-----|-----|
| **socket 类型** | `SOCK_STREAM` | `SOCK_DGRAM` |
| **listen/accept** | 需要 | **不需要** |
| **发送** | `send()` | `sendto()`（需要目标地址） |
| **接收** | `recv()` | `recvfrom()`（返回发送方地址） |

**UDP Send() 实现：**
```cpp
int CUdpSocket::Send(const Buffer& data) {
    return sendto(m_socket, data, data.size(), 0,
                  m_param.addrin(), sizeof(sockaddr_in));
    //            ↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑
    //            比 send() 多了目标地址参数
}
```

---

### 5.3 三种 Socket 完整对比表

| 特性 | CLocalSocket | CTcpSocket | CUdpSocket |
|------|-------------|-----------|-----------|
| **地址族** | AF_UNIX | AF_INET | AF_INET |
| **socket 类型** | SOCK_STREAM | SOCK_STREAM | SOCK_DGRAM |
| **绑定前操作** | unlink() | 无 | 无 |
| **地址结构** | sockaddr_un | sockaddr_in | sockaddr_in |
| **listen** | ✅ | ✅ | ❌ |
| **accept** | ✅ | ✅ | ❌ |
| **connect** | ✅ | ✅ | ❌ |
| **发送** | send() | send() | sendto() |
| **接收** | recv() | recv() | recvfrom() |
| **应用场景** | 本地进程通信 | 网络可靠传输 | 网络快速传输 |

---

## 六、关键概念理解

### 6.1 服务器的两个 Socket

**易混淆点：**
```cpp
// 服务器端
CLocalSocket* server = new CLocalSocket();  // 监听 socket
server->Init(...);

CLocalSocket* client = NULL;  // 连接 socket
server->Link(&client);        // client 是服务器端的对象！
```

**client 对象的本质：**
- ❌ **不是**客户端进程的对象
- ✅ **是**服务器端用来和客户端通信的对象
- ✅ **持有**连接到客户端的文件描述符（句柄）

**完整通信架构：**
```
┌─────────────────────┐        ┌─────────────────────┐
│   服务器进程         │        │   客户端进程         │
│                     │        │                     │
│  server（监听）      │        │  client（连接）      │
│  fd = 3             │        │  fd = 3             │
│      ↓ accept()     │        │      ↓ connect()    │
│  client（连接）      │←━━━━━━→│                     │
│  fd = 4             │        │                     │
└─────────────────────┘        └─────────────────────┘
     ↑                                  ↑
     └──── 通过内核缓冲区交换数据 ───────┘
```

---

### 6.2 bind() 函数的三个参数

```cpp
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
```

| 参数 | 作用 | Unix 域示例 | TCP 示例 |
|------|------|------------|---------|
| **sockfd** | socket 文件描述符 | `m_socket` | `m_socket` |
| **addr** | 地址信息指针 | `param.addrun()` | `param.addrin()` |
| **addrlen** | 地址结构体大小 | `sizeof(sockaddr_un)` | `sizeof(sockaddr_in)` |

---

### 6.3 fcntl() 设置非阻塞

```cpp
int flags = fcntl(m_socket, F_GETFL, 0);
//                          ↑↑↑↑↑↑
//                          F_GETFL：获取文件标志

fcntl(m_socket, F_SETFL, flags | O_NONBLOCK);
//              ↑↑↑↑↑↑   ↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑
//              F_SETFL   原标志 | 非阻塞标志
```

**为什么要分两步？**
- 保留原有标志（如读写模式）
- 只添加非阻塞标志
- 避免覆盖其他设置

---

## 七、关键问题思考

### Q1: 为什么 Link() 要用双重指针？

**A：** 需要"返回"新创建的客户端对象
- 返回值已经用于表示成功/失败
- 双重指针可以修改外部指针变量

---

### Q2: 为什么服务器端也有 client 对象？

**A：** accept() 返回新的 fd
- 需要封装成对象方便管理
- 它代表"与客户端的连接"

---

### Q3: 为什么 Close() 要先设 -1？

**A：** 防止异常/中断导致重复关闭
- 保证状态一致性
- 即使 close() 失败也安全

---

### Q4: 当前实现能解决粘包/半包吗？

**A：** 不能，只是基础封装
- 需要在上层实现协议层
- 常用方案：长度前缀协议

---

## 八、学习收获

### 8.1 技术理解

1. **Socket 通信本质**
   - 文件描述符是通信的"句柄"
   - 服务器有两个 socket（监听 + 连接）
   - 通信通过内核缓冲区交换数据

2. **C++ 封装技巧**
   - 抽象基类定义统一接口
   - RAII 管理资源生命周期
   - 运算符重载简化使用

3. **协议差异**
   - Unix 域：本地、需要 unlink
   - TCP：可靠、有连接、send/recv
   - UDP：快速、无连接、sendto/recvfrom

---

### 8.2 代码统计

| 类 | 函数数量 | 代码行数 |
|----|---------|---------|
| **Buffer** | 3个构造+3个运算符 | ~10行 |
| **CSockParam** | 3个构造+2个辅助 | ~50行 |
| **CSocketBase** | 5个纯虚函数+1个析构 | ~20行 |
| **CLocalSocket** | 5个实现函数 | ~100行 |
| **CTcpSocket** | 5个实现函数 | ~100行 |
| **CUdpSocket** | 5个实现函数 | ~80行 |
| **总计** | - | **~360行** |

---

## 九、后续计划

### 已完成
- ✅ Buffer 类封装
- ✅ CSockParam 参数管理
- ✅ CSocketBase 抽象基类
- ✅ CLocalSocket 完整实现
- ✅ CTcpSocket 完整实现
- ✅ CUdpSocket 完整实现

### 下一步
- [ ] 编写测试程序（服务器/客户端通信）
- [ ] 体验粘包/半包问题
- [ ] 设计应用层协议（长度前缀）
- [ ] 实现 CProtocol 类
- [ ] 结合 Epoll 实现高并发服务器

---

## 参考资料

- Linux Man Pages: `man socket`, `man bind`, `man listen`
- 参考项目：易播服务器代码
  - 017：接口定义
  - 018：本地套接字实现
- 系统调用：`socket()`, `bind()`, `listen()`, `accept()`, `connect()`

---

**学习日期：** 2025-11-23
**项目状态：** Socket 封装完成 ✅
**下一步：** 编写测试程序，验证通信功能
