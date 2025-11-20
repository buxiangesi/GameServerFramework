# Socket封装学习总结

## 项目信息

**学习日期**：2025-11-20
**学习项目**：易播服务器 - Socket网络通信封装
**学习目标**：掌握Linux Socket编程的面向对象封装，理解网络编程设计模式

**参考代码路径**：
- 017章节（接口定义）：`C:\Users\王万鑫\Desktop\易播\易播服务器\代码\017-易播-套接字接口类封装\EPlayerServer`
- 018章节（完整实现）：`C:\Users\王万鑫\Desktop\易播\易播服务器\代码\018-易播-本地套接字实现\EPlayerServer`

**当前项目路径**：`D:\VS\learning_linux_11_6\learning_linux_11_6`

---

## 目录

1. [整体架构设计](#一整体架构设计)
2. [技术选型与设计思路](#二技术选型与设计思路)
3. [Buffer类详解](#三buffer类详解)
4. [CSockParam类详解](#四csockparam类详解)
5. [SockAttr属性枚举](#五sockattr属性枚举)
6. [CSocketBase抽象基类](#六csocketbase抽象基类)
7. [CLocalSocket实现](#七clocalsocket实现)
8. [设计模式应用](#八设计模式应用)
9. [关键问题与答案](#九关键问题与答案)
10. [学习进度与计划](#十学习进度与计划)

---

## 一、整体架构设计

### 1.1 易播服务器三大核心组件

```
┌─────────────────────────────────────────────────────┐
│              易播服务器（EPlayerServer）              │
│          高性能多进程网络服务器架构                   │
└─────────────────────────────────────────────────────┘
                        │
        ┌───────────────┼───────────────┐
        ↓               ↓               ↓
   ┌─────────┐     ┌─────────┐     ┌─────────┐
   │ Socket  │     │  Epoll  │     │ Process │
   │ 网络通信 │     │ I/O复用 │     │ 进程管理 │
   └─────────┘     └─────────┘     └─────────┘
        │               │               │
        └───────────────┴───────────────┘
                        ↓
            ┌────────────────────────┐
            │  整合：多进程服务器      │
            │  Socket + Epoll         │ ← 单进程高性能
            │  Socket + Epoll + Process│ ← 多进程负载均衡
            └────────────────────────┘
```

**三大组件的职责**：

| 组件 | 职责 | 状态 | 核心API |
|------|------|------|---------|
| **Socket** | 网络连接、数据收发 | 🔄 学习中 | socket, bind, listen, accept, connect |
| **Epoll** | I/O事件监听、高并发 | ✅ 已完成 | epoll_create, epoll_ctl, epoll_wait |
| **Process** | 多进程管理、负载均衡 | ✅ 已完成 | fork, socketpair, sendmsg/recvmsg |

---

### 1.2 Socket封装的内部架构

```
┌──────────────────────────────────────────────────────┐
│                   Socket封装架构                      │
└──────────────────────────────────────────────────────┘

【辅助工具层】
┌──────────────┐  ┌──────────────┐  ┌──────────────┐
│    Buffer    │  │  SockAttr    │  │  CSockParam  │
│  智能缓冲区   │  │  属性枚举     │  │  参数封装     │
└──────────────┘  └──────────────┘  └──────────────┘
       │                  │                  │
       └──────────────────┴──────────────────┘
                          ↓
           【抽象接口层 - 策略模式核心】
        ┌──────────────────────────────────┐
        │       CSocketBase                │
        │  (抽象基类 - 定义统一接口)        │
        │  ├─ Init()   : 初始化             │
        │  ├─ Link()   : 建立连接           │
        │  ├─ Send()   : 发送数据           │
        │  ├─ Recv()   : 接收数据           │
        │  └─ Close()  : 关闭连接           │
        └──────────────────────────────────┘
                          ↓
           【具体实现层 - 多态实现】
        ┌──────────┬──────────┬──────────┐
        │          │          │          │
        ↓          ↓          ↓          ↓
  ┌──────────┐ ┌──────────┐ ┌──────────┐
  │ CLocal   │ │  CTcp    │ │  CUdp    │
  │ Socket   │ │ Socket   │ │ Socket   │
  │          │ │          │ │          │
  │ Unix域   │ │ TCP网络  │ │ UDP网络  │
  │ 本机IPC  │ │ 面向连接 │ │ 无连接   │
  └──────────┘ └──────────┘ └──────────┘
      ✅            ⏳          ⏳
    已完成        待实现      待实现
```

**架构设计的核心思想**：
- **分层设计**：工具层 → 接口层 → 实现层，职责清晰
- **策略模式**：抽象接口统一，具体实现可替换
- **面向接口编程**：上层代码只依赖CSocketBase接口

---

### 1.3 三种Socket实现的对比

| 特性 | CLocalSocket | CTcpSocket | CUdpSocket |
|------|-------------|-----------|-----------|
| **用途** | 本机进程通信 | 网络TCP通信 | 网络UDP通信 |
| **协议族** | `PF_LOCAL` / `AF_UNIX` | `PF_INET` | `PF_INET` |
| **类型** | `SOCK_STREAM` / `SOCK_DGRAM` | `SOCK_STREAM` | `SOCK_DGRAM` |
| **地址结构** | `sockaddr_un` | `sockaddr_in` | `sockaddr_in` |
| **地址内容** | 文件路径 | IP + 端口 | IP + 端口 |
| **连接性** | 可连接（TCP模式） | 面向连接 | 无连接 |
| **可靠性** | 可靠 | 可靠、有序 | 不可靠、无序 |
| **性能** | ⭐⭐⭐（最快） | ⭐⭐（稍慢） | ⭐⭐⭐（快） |
| **使用场景** | 守护进程通信、同机服务 | Web服务器、数据库 | 实时视频、DNS |
| **实现难度** | 参考代码✅ | 95%相似 | 70%相似 |

**代码相似度分析**：
```
CLocalSocket (100% - 参考实现)
    │
    ├─ CTcpSocket (95%相似)
    │   改动：PF_LOCAL→PF_INET, addrun()→addrin(), sockaddr_un→sockaddr_in
    │
    └─ CUdpSocket (70%相似)
        改动：无连接、Link()空实现、Send用sendto()、Recv用recvfrom()
```

---

### 1.4 Socket + Epoll + Process 整合架构

#### 单进程高性能服务器（Socket + Epoll）

```
┌────────────────────────────────────────┐
│          主进程                         │
│                                        │
│  ┌──────────┐      ┌──────────┐       │
│  │  Server  │─────>│  Epoll   │       │
│  │  Socket  │      │          │       │
│  └──────────┘      └──────────┘       │
│       │                 │              │
│       │  新连接          │  监听事件     │
│       ↓                 ↓              │
│  ┌──────────┐      ┌──────────┐       │
│  │ accept() │      │  Wait()  │       │
│  └──────────┘      └──────────┘       │
│       │                 │              │
│       └────────┬────────┘              │
│                ↓                       │
│       处理客户端请求                     │
│       (事件循环)                        │
└────────────────────────────────────────┘

适用场景：中小规模并发（<10000连接）
优点：简单、资源占用少
缺点：单核CPU，无法利用多核
```

#### 多进程负载均衡服务器（Socket + Epoll + Process）

```
┌────────────────────────────────────────────────────┐
│                    父进程                           │
│  ┌──────────┐                                      │
│  │  Server  │ listen(fd=3)                        │
│  │  Socket  │                                      │
│  └──────────┘                                      │
│       │                                            │
│       │ SendFD(3)       SendFD(3)       SendFD(3) │
│       ├─────────────────┬────────────────┬────────┤
└───────┼─────────────────┼────────────────┼────────┘
        ↓                 ↓                ↓
┌───────────────┐  ┌───────────────┐  ┌───────────────┐
│  子进程1       │  │  子进程2       │  │  子进程3       │
│               │  │               │  │               │
│ RecvFD(3)     │  │ RecvFD(3)     │  │ RecvFD(3)     │
│      │        │  │      │        │  │      │        │
│  ┌───↓───┐   │  │  ┌───↓───┐   │  │  ┌───↓───┐   │
│  │ Epoll │   │  │  │ Epoll │   │  │  │ Epoll │   │
│  └───────┘   │  │  └───────┘   │  │  └───────┘   │
│      │        │  │      │        │  │      │        │
│   accept()    │  │   accept()    │  │   accept()    │
│   事件循环     │  │   事件循环     │  │   事件循环     │
│               │  │               │  │               │
│  CPU核心1     │  │  CPU核心2     │  │  CPU核心3     │
└───────────────┘  └───────────────┘  └───────────────┘

适用场景：高并发场景（>10000连接）
优点：充分利用多核CPU，负载均衡
关键技术：
  1. 父进程创建监听socket
  2. CProcess::SendFD()传递socket给子进程
  3. 多子进程同时accept（内核负载均衡）
  4. 每个子进程用Epoll管理自己的连接
```

**技术亮点**：
- **惊群效应避免**：Linux内核自动处理accept惊群（2.6+）
- **负载均衡**：内核轮流唤醒子进程accept
- **故障隔离**：子进程崩溃不影响其他进程

---

## 二、技术选型与设计思路

### 2.1 为什么要封装Socket？

**原生Socket API的问题**：

| 问题 | 原生API | 封装后 |
|------|---------|--------|
| **代码冗长** | 每次10+行初始化代码 | 1行`Init(param)` |
| **参数复杂** | 记忆6+个参数和顺序 | 参数类封装 |
| **容易出错** | 忘记htons、bind、listen | 封装内处理 |
| **协议差异** | TCP/UDP/Unix域代码完全不同 | 统一接口 |
| **难以维护** | 重复代码多 | 复用基类 |
| **资源管理** | 手动close，易泄漏 | RAII自动管理 |

**示例对比**：

```cpp
// ❌ 原生方式：20+行代码
int fd = socket(AF_UNIX, SOCK_STREAM, 0);
struct sockaddr_un addr;
memset(&addr, 0, sizeof(addr));
addr.sun_family = AF_UNIX;
strcpy(addr.sun_path, "/tmp/test.sock");
unlink("/tmp/test.sock");  // 删除旧文件
int ret = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
if (ret == -1) { perror("bind"); return -1; }
ret = listen(fd, 32);
if (ret == -1) { perror("listen"); return -1; }
// ... 还要手动设置非阻塞
// ... 最后记得close(fd)

// ✅ 封装方式：3行代码
CSockParam param("/tmp/test.sock", SOCK_ISSERVER | SOCK_ISNONBLOCK);
CLocalSocket server;
server.Init(param);  // 完成所有初始化
// 析构时自动close
```

---

### 2.2 面向对象设计的核心思路

#### 设计原则

| 原则 | 应用 | 体现 |
|------|------|------|
| **单一职责** | 每个类只做一件事 | Buffer管缓冲、CSockParam管参数、CSocketBase管生命周期 |
| **开闭原则** | 对扩展开放，对修改关闭 | 新增socket类型不需修改基类 |
| **里氏替换** | 子类可以替换父类 | `CSocketBase* sock = new CTcpSocket()` |
| **接口隔离** | 接口最小化 | 5个纯虚函数，职责明确 |
| **依赖倒置** | 依赖抽象而非具体 | 上层代码依赖CSocketBase接口 |
| **RAII** | 资源获取即初始化 | 构造时分配，析构时释放 |

#### 分层架构的优势

```
用户代码
    ↓ (只依赖接口)
CSocketBase 接口层
    ↓ (实现细节隔离)
具体实现层 (CLocalSocket/CTcpSocket/CUdpSocket)
```

**好处**：
1. **易于测试**：可以mock CSocketBase接口
2. **易于扩展**：添加新协议不影响现有代码
3. **易于维护**：修改实现不影响接口
4. **代码复用**：子类复用基类逻辑

---

### 2.3 关键技术选型

#### 选型1：为什么继承std::string实现Buffer？

**备选方案对比**：

| 方案 | 优点 | 缺点 | 选择 |
|------|------|------|------|
| **继承std::string** | 免费获得所有string功能 | 轻微性能开销 | ✅ 推荐 |
| 包含std::string成员 | 更安全（组合优于继承） | 需要转发所有函数 | ❌ 太繁琐 |
| 手写动态数组 | 完全可控 | 重复造轮子，易出错 | ❌ 不推荐 |
| 直接用char* | 性能最好 | 手动管理内存，易泄漏 | ❌ 不安全 |

**为什么选择继承？**
```cpp
// 继承后免费获得：
Buffer buf;
buf.size();        // std::string的size()
buf.resize(1024);  // std::string的resize()
buf.clear();       // std::string的clear()
buf += "hello";    // std::string的operator+=
buf.find("world"); // std::string的find()
// ... 还有几十个函数！

// 只需添加：
operator char*()   // 适配socket API
```

---

#### 选型2：为什么用位标志而非bool参数？

**备选方案对比**：

```cpp
// ❌ 方案1：多个bool参数
CSockParam(ip, port, bool is_server, bool is_nonblock, bool is_udp);
//                   ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
//                   问题：参数顺序难记，容易传错

// ❌ 方案2：枚举类型
enum ServerType { CLIENT, SERVER };
enum BlockType { BLOCK, NONBLOCK };
enum ProtoType { TCP, UDP };
CSockParam(ip, port, ServerType st, BlockType bt, ProtoType pt);
//                   ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
//                   问题：参数太多

// ✅ 方案3：位标志（最终选择）
CSockParam(ip, port, SOCK_ISSERVER | SOCK_ISNONBLOCK | SOCK_ISUDP);
//                   ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
//                   优点：一个参数组合多个属性
```

**位标志的优势**：
1. **可扩展**：添加新属性不改变接口
2. **组合灵活**：用`|`运算符自由组合
3. **性能好**：位运算高效
4. **代码简洁**：一个int存储多个bool

---

#### 选型3：为什么用双重指针而非引用？

**Link函数参数的演化**：

```cpp
// ❌ 尝试1：用返回值
CSocketBase* Link();
// 问题：返回值已被占用（0表示成功，负数表示错误）

// ❌ 尝试2：用单指针
int Link(CSocketBase* pClient);
// 问题：无法修改外部变量（值传递）

// ✅ 尝试3：用双重指针
int Link(CSocketBase** pClient = NULL);
// 优点：可以修改外部变量，NULL表示客户端

// 🤔 尝试4：用引用
int Accept(CSocketBase*& pClient);  // 服务器
int Connect();  // 客户端
// 优点：C++风格，类型安全
// 缺点：需要两个函数，子类要实现两个
```

**为什么参考代码选择双重指针？**
1. **兼容性**：C++98标准
2. **简洁性**：一个函数搞定服务器和客户端
3. **可判断**：用NULL区分服务器/客户端

---

#### 选型4：为什么需要状态机？

**状态机的作用**：

```cpp
// 没有状态机的问题：
CLocalSocket sock;
sock.Init(param);
sock.Init(param);  // ❌ 重复初始化，fd泄漏
sock.Send(data);   // ❌ 还没连接就发送，崩溃

// 有状态机的保护：
sock.Init(param);       // m_status: 0 → 1
sock.Init(param);       // ❌ m_status != 0，返回-1
sock.Link();            // m_status: 1 → 2
sock.Send(data);        // ✅ m_status == 2，允许
```

**状态机设计**：
```
状态0（未初始化）
    ↓ Init()
状态1（已初始化）
    ↓ Link()
状态2（已连接）
    ↓ Close()
状态3（已关闭）
```

---

## 三、Buffer类详解

### 3.1 设计目标

**核心目标**：提供一个既有std::string的便利，又能直接传给socket API的缓冲区类。

**解决的痛点**：
```cpp
// 痛点1：固定大小缓冲区
char buf[1024];  // 太小不够用，太大浪费内存

// 痛点2：手动管理内存
char* buf = (char*)malloc(size);
// ... 使用
free(buf);  // 忘记就泄漏

// 痛点3：std::string不能直接传给socket
std::string data = "hello";
send(fd, data, data.size(), 0);  // ❌ 编译错误
send(fd, data.c_str(), data.size(), 0);  // ✅ 但每次都要转换
```

---

### 3.2 完整实现与详解

```cpp
class Buffer : public std::string
{
public:
    // 默认构造函数
    Buffer() : std::string() {}

    // 指定大小构造函数
    // 用途：预分配缓冲区，避免多次内存分配
    // 示例：Buffer buf(4096);  // 预分配4KB
    Buffer(size_t size) : std::string() {
        resize(size);  // std::string的resize函数
    }

    // ==================== 三个类型转换运算符 ====================

    // 版本1：非const对象 → char*（可写）
    // 使用场景：recv/read等需要写入数据的场景
    // 示例：recv(fd, buf, buf.size(), 0);
    operator char*() {
        return (char*)c_str();  // 去掉const限定
    }

    // 版本2：const对象 → char*（兼容老API）
    // 使用场景：某些老API参数声明为char*但实际不修改
    // 示例：void legacy_print(char* str);  legacy_print(cbuf);
    operator char*() const {
        return (char*)c_str();  // const对象也能转char*
    }

    // 版本3：const对象 → const char*（只读，最安全）
    // 使用场景：printf、strcmp等只读API
    // 示例：printf("%s\n", buf);
    operator const char*() const {
        return c_str();  // 标准的只读转换
    }
};
```

---

### 3.3 三个转换运算符的深度理解

#### 问题：为什么需要三个版本？

**编译器重载决议规则**：

```cpp
Buffer buf;              // 非const对象
const Buffer cbuf;       // const对象

// 场景1：recv需要写入
recv(fd, buf, buf.size(), 0);
// 编译器匹配：非const对象 + void*参数
// 调用：operator char*()（版本1）

// 场景2：printf只读
printf("%s", cbuf);
// 编译器匹配：const对象 + const char*参数
// 调用：operator const char*() const（版本3）

// 场景3：老API（char*但不修改）
void old_api(char* str) { printf("%s", str); }
old_api(cbuf);
// 编译器匹配：const对象 + char*参数
// 调用：operator char*() const（版本2）
```

**如果少一个版本会怎样？**

```cpp
// 假设只有版本1和版本3
class Buffer : public std::string {
    operator char*() { return (char*)c_str(); }  // 版本1
    operator const char*() const { return c_str(); }  // 版本3
    // 缺少版本2
};

const Buffer cbuf = "hello";
void old_api(char* str);
old_api(cbuf);  // ❌ 编译错误！
// 错误：const对象不能调用非const成员函数（版本1）
// 版本3类型不匹配（const char* vs char*）
```

---

### 3.4 实际应用示例

```cpp
// 示例1：动态大小接收
Buffer recv_buf(4096);  // 预分配4KB
int n = recv(fd, recv_buf, recv_buf.size(), 0);
if (n > 0) {
    recv_buf.resize(n);  // 调整为实际大小
    printf("收到: %s\n", (const char*)recv_buf);
}

// 示例2：字符串拼接
Buffer response = "HTTP/1.1 200 OK\r\n";
response += "Content-Type: text/html\r\n";
response += "\r\n<html>Hello</html>";
send(fd, response, response.size(), 0);

// 示例3：查找和替换
Buffer data = "Hello World";
size_t pos = data.find("World");  // std::string的find
if (pos != std::string::npos) {
    data.replace(pos, 5, "C++");  // std::string的replace
}
```

---

### 3.5 设计权衡

**继承的风险**：
```cpp
// ⚠️ 问题：std::string没有虚析构
std::string* p = new Buffer();
delete p;  // ❌ 未定义行为（但Buffer没有额外资源，影响小）
```

**为什么可以接受？**
1. Buffer没有额外的资源需要释放
2. 使用时不会用std::string*指向Buffer对象
3. 实际使用都是栈对象或`Buffer*`指针

**更安全的替代方案（如果要严格遵守规范）**：
```cpp
// 组合而非继承
class Buffer {
private:
    std::string m_data;
public:
    operator char*() { return (char*)m_data.c_str(); }
    // 需要转发所有std::string的函数（太繁琐）
    size_t size() const { return m_data.size(); }
    void resize(size_t n) { m_data.resize(n); }
    // ... 几十个函数
};
```

**结论**：在实际项目中，继承std::string是可接受的权衡。

---

## 四、CSockParam类详解

### 4.1 设计目标

**核心目标**：统一封装TCP/UDP/Unix域socket的初始化参数。

**设计挑战**：
```
TCP/UDP需要：IP地址 + 端口号
Unix域需要：文件路径
如何用一个类同时支持？
```

**解决方案**：同时存储两种地址结构，根据构造函数决定用哪个。

---

### 4.2 完整结构分析

```cpp
class CSockParam {
public:
    // ==================== 5个成员变量 ====================

    sockaddr_in addr_in;   // TCP/UDP地址结构（16字节）
    sockaddr_un addr_un;   // Unix域地址结构（110字节）
    Buffer ip;             // IP地址或Unix路径字符串
    short port;            // 端口号（-1表示无效）
    int attr;              // 属性标志（位标志组合）

    // ==================== 3个构造函数 ====================

    // ① 默认构造：创建空对象
    CSockParam() {
        bzero(&addr_in, sizeof(addr_in));  // 清零TCP地址
        bzero(&addr_un, sizeof(addr_un));  // 清零Unix地址
        port = -1;   // 无效端口
        attr = 0;    // 无属性
    }

    // ② TCP/UDP构造：网络通信
    CSockParam(const Buffer& ip, short port, int attr) {
        this->ip = ip;
        this->port = port;
        this->attr = attr;

        // 填充TCP/UDP地址结构
        addr_in.sin_family = AF_INET;              // IPv4
        addr_in.sin_port = port;                   // 端口（注意：应该htons）
        addr_in.sin_addr.s_addr = inet_addr(ip);   // IP字符串→整数
    }

    // ③ Unix域构造：本机通信
    CSockParam(const Buffer& path, int attr) {
        ip = path;  // 复用ip字段存路径

        // 填充Unix域地址结构
        addr_un.sun_family = AF_UNIX;    // Unix域
        strcpy(addr_un.sun_path, path);  // 复制路径字符串

        this->attr = attr;
    }

    // ==================== 辅助函数 ====================

    // 返回TCP/UDP地址指针（类型转换为sockaddr*）
    sockaddr* addrin() { return (sockaddr*)&addr_in; }

    // 返回Unix域地址指针
    sockaddr* addrun() { return (sockaddr*)&addr_un; }
};
```

---

### 4.3 两个地址结构体的详解

#### TCP/UDP地址：sockaddr_in

```cpp
struct sockaddr_in {
    sa_family_t sin_family;      // 地址族：AF_INET（IPv4）
    in_port_t sin_port;          // 端口号：网络字节序（大端）
    struct in_addr sin_addr;     // IP地址
    char sin_zero[8];            // 填充字节（对齐）
};

struct in_addr {
    uint32_t s_addr;  // IP地址：32位整数，网络字节序
};
```

**示例**：
```cpp
CSockParam param("192.168.1.100", 8080, SOCK_ISSERVER);

// 内存布局：
// addr_in.sin_family = 2 (AF_INET)
// addr_in.sin_port = 8080 (0x1F90)
// addr_in.sin_addr.s_addr = 0x6401A8C0 (网络字节序)
//   192.168.1.100 → 0xC0A80164 (主机序) → 0x6401A8C0 (网络序)
```

---

#### Unix域地址：sockaddr_un

```cpp
struct sockaddr_un {
    sa_family_t sun_family;  // 地址族：AF_UNIX
    char sun_path[108];      // 文件路径（最大108字节）
};
```

**示例**：
```cpp
CSockParam param("/tmp/myserver.sock", SOCK_ISSERVER);

// 内存布局：
// addr_un.sun_family = 1 (AF_UNIX)
// addr_un.sun_path = "/tmp/myserver.sock\0"
```

---

### 4.4 关键问题：为什么Unix域用strcpy？

**核心原因**：数据类型不同

```cpp
// TCP的IP地址：uint32_t整数类型
addr_in.sin_addr.s_addr = inet_addr("192.168.1.1");
//                      ^
//                      直接赋值（整数 = 整数）

// Unix域的路径：char[108]数组类型
strcpy(addr_un.sun_path, "/tmp/test.sock");
//    ^^^^^^
//    必须用字符串复制函数（数组不能直接赋值）
```

**为什么数组不能直接赋值？**

```cpp
char arr[10];
arr = "hello";  // ❌ 编译错误：invalid array assignment

// 原因：数组名是一个地址常量
char arr[10];
// arr 的类型是 char[10]
// 但作为右值时，退化为 char* const（常量指针）
// 不能修改指针本身的值

// 正确做法：复制内容
strcpy(arr, "hello");  // ✅ 复制字符串内容到数组
```

**C++的本质**：
```cpp
int a[10];
a = ...;  // ❌ 不能改变数组的起始地址

int* p = a;
p = ...;  // ✅ 可以改变指针的指向
```

---

### 4.5 构造函数的设计技巧

#### 技巧1：用bzero而非memset

```cpp
// bzero vs memset
bzero(&addr_in, sizeof(addr_in));      // BSD风格，简洁
memset(&addr_in, 0, sizeof(addr_in));  // POSIX风格，通用

// 为什么用bzero？
// 1. 简洁：2个参数 vs 3个参数
// 2. 不会搞错参数顺序
// 3. 语义明确（zero = 清零）
```

---

#### 技巧2：port=-1表示无效

```cpp
CSockParam() {
    // ...
    port = -1;  // -1表示无效端口
}

// 使用场景：
CSockParam param;
if (param.port == -1) {
    // 未初始化，需要后续赋值
}
```

---

#### 技巧3：复用ip字段存路径

```cpp
// Unix域构造函数
CSockParam(const Buffer& path, int attr) {
    ip = path;  // 复用ip字段存储路径
    // ...
}

// 为什么可以复用？
// 因为Unix域不需要IP地址，ip字段空闲
// 统一接口：param.ip既能存IP，也能存路径
```

---

### 4.6 实际应用示例

```cpp
// 场景1：TCP服务器
CSockParam tcp_server("0.0.0.0", 8080, SOCK_ISSERVER | SOCK_ISNONBLOCK);
printf("IP: %s, Port: %d\n", tcp_server.ip.c_str(), tcp_server.port);
// 输出：IP: 0.0.0.0, Port: 8080

// 场景2：TCP客户端
CSockParam tcp_client("192.168.1.100", 8080, 0);
// attr=0：既不是服务器，也不是非阻塞

// 场景3：Unix域服务器
CSockParam unix_server("/tmp/myserver.sock", SOCK_ISSERVER);
printf("Path: %s\n", unix_server.ip.c_str());
// 输出：Path: /tmp/myserver.sock

// 场景4：拷贝参数
CSockParam param1("127.0.0.1", 8080, SOCK_ISSERVER);
CSockParam param2 = param1;  // 拷贝构造
CSockParam param3;
param3 = param1;  // 赋值运算符
```

---

### 4.7 设计不足与改进

#### 不足1：没有使用htons转换端口

```cpp
// 当前实现
addr_in.sin_port = port;  // ❌ 应该转换字节序

// 正确实现
addr_in.sin_port = htons(port);  // ✅ 主机序→网络序
```

**为什么需要htons？**
```
网络协议规定：网络字节序 = 大端序（Big Endian）
x86架构：主机字节序 = 小端序（Little Endian）

端口8080：
  主机序：0x1F90（内存：90 1F）
  网络序：0x901F（内存：90 1F）← htons转换

如果不转换：
  本机测试可能正常（loopback自动处理）
  跨机器通信会出错（端口号错误）
```

---

#### 不足2：strcpy可能溢出

```cpp
// 当前实现
strcpy(addr_un.sun_path, path);  // ⚠️ 如果path>108字节会溢出

// 安全实现
strncpy(addr_un.sun_path, path, sizeof(addr_un.sun_path) - 1);
addr_un.sun_path[sizeof(addr_un.sun_path) - 1] = '\0';
```

---

#### 改进建议

```cpp
CSockParam(const Buffer& ip, short port, int attr) {
    this->ip = ip;
    this->port = port;
    this->attr = attr;

    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(port);  // ✅ 添加htons
    addr_in.sin_addr.s_addr = inet_addr(ip);
}

CSockParam(const Buffer& path, int attr) {
    ip = path;
    addr_un.sun_family = AF_UNIX;

    // ✅ 安全的字符串复制
    strncpy(addr_un.sun_path, path, sizeof(addr_un.sun_path) - 1);
    addr_un.sun_path[sizeof(addr_un.sun_path) - 1] = '\0';

    this->attr = attr;
}
```

---

## 五、SockAttr属性枚举

### 5.1 位标志设计原理

**核心思想**：用一个整数的不同位表示不同的属性。

```cpp
enum SockAttr {
    SOCK_ISSERVER   = 1,  // 0001 - bit 0
    SOCK_ISNONBLOCK = 2,  // 0010 - bit 1
    SOCK_ISUDP      = 4,  // 0100 - bit 2
    // 可扩展：
    // SOCK_ISREUSE     = 8,   // 1000 - bit 3
    // SOCK_KEEPALIVE   = 16,  // 10000 - bit 4
};
```

**每个标志占用一个bit位**：
```
bit 0: 服务器/客户端
bit 1: 阻塞/非阻塞
bit 2: TCP/UDP
bit 3: 端口复用（未来）
bit 4: 保持连接（未来）
...
```

---

### 5.2 位运算操作详解

#### 组合标志：| 运算符

```cpp
int attr = SOCK_ISSERVER | SOCK_ISNONBLOCK | SOCK_ISUDP;

// 二进制运算过程：
  0001  (SOCK_ISSERVER)
| 0010  (SOCK_ISNONBLOCK)
------
  0011
| 0100  (SOCK_ISUDP)
------
  0111  (结果：attr = 7)

// attr的二进制：0111
// 解读：bit 0=1（服务器），bit 1=1（非阻塞），bit 2=1（UDP）
```

---

#### 检查标志：& 运算符

```cpp
int attr = SOCK_ISSERVER | SOCK_ISNONBLOCK;  // 0011

// 检查是否是服务器
if (attr & SOCK_ISSERVER) {
    //  0011 (attr)
    //& 0001 (SOCK_ISSERVER)
    //------
    //  0001 (非0，条件为真)
    printf("是服务器\n");
}

// 检查是否非阻塞
if (attr & SOCK_ISNONBLOCK) {
    //  0011 (attr)
    //& 0010 (SOCK_ISNONBLOCK)
    //------
    //  0010 (非0，条件为真)
    printf("是非阻塞\n");
}

// 检查是否UDP
if (attr & SOCK_ISUDP) {
    //  0011 (attr)
    //& 0100 (SOCK_ISUDP)
    //------
    //  0000 (为0，条件为假)
    printf("是UDP\n");  // 不会执行
}
```

---

#### 清除标志：&~ 运算符

```cpp
int attr = SOCK_ISSERVER | SOCK_ISNONBLOCK;  // 0011

// 清除非阻塞标志
attr = attr & ~SOCK_ISNONBLOCK;
//     0011 & ~0010
//     0011 & 1101
//     0001 (结果：只剩服务器标志)
```

---

#### 翻转标志：^ 运算符

```cpp
int attr = SOCK_ISSERVER;  // 0001

// 翻转非阻塞标志
attr = attr ^ SOCK_ISNONBLOCK;
//     0001 ^ 0010
//     0011 (添加了非阻塞)

attr = attr ^ SOCK_ISNONBLOCK;
//     0011 ^ 0010
//     0001 (移除了非阻塞)
```

---

### 5.3 八种组合示例

```cpp
// 组合1：客户端 + 阻塞 + TCP
int attr1 = 0;  // 000
// 默认：客户端、阻塞、TCP

// 组合2：服务器 + 阻塞 + TCP
int attr2 = SOCK_ISSERVER;  // 001

// 组合3：客户端 + 非阻塞 + TCP
int attr3 = SOCK_ISNONBLOCK;  // 010

// 组合4：服务器 + 非阻塞 + TCP
int attr4 = SOCK_ISSERVER | SOCK_ISNONBLOCK;  // 011

// 组合5：客户端 + 阻塞 + UDP
int attr5 = SOCK_ISUDP;  // 100

// 组合6：服务器 + 阻塞 + UDP
int attr6 = SOCK_ISSERVER | SOCK_ISUDP;  // 101

// 组合7：客户端 + 非阻塞 + UDP
int attr7 = SOCK_ISNONBLOCK | SOCK_ISUDP;  // 110

// 组合8：服务器 + 非阻塞 + UDP
int attr8 = SOCK_ISSERVER | SOCK_ISNONBLOCK | SOCK_ISUDP;  // 111
```

---

### 5.4 为什么值必须是2的幂次？

**错误示例**：
```cpp
// ❌ 错误设计
enum SockAttr {
    SOCK_ISSERVER = 1,   // 0001
    SOCK_ISNONBLOCK = 3, // 0011 ← 错误！
};

int attr = SOCK_ISSERVER | SOCK_ISNONBLOCK;
// 0001 | 0011 = 0011

// 问题：无法区分
attr == 3  // 是 "服务器+非阻塞" 还是 "只有ISNONBLOCK" ？
```

**正确设计**：
```cpp
// ✅ 正确：必须是2的幂次
SOCK_ISSERVER   = 1 (2^0)  // 0001
SOCK_ISNONBLOCK = 2 (2^1)  // 0010
SOCK_ISUDP      = 4 (2^2)  // 0100

// 每个标志占用独立的bit位，不会冲突
```

---

### 5.5 实际应用

```cpp
// 在Init函数中使用
int CLocalSocket::Init(const CSockParam& param) {
    // 根据UDP标志选择socket类型
    int type = (param.attr & SOCK_ISUDP) ? SOCK_DGRAM : SOCK_STREAM;

    m_socket = socket(PF_LOCAL, type, 0);

    // 服务器需要bind+listen
    if (param.attr & SOCK_ISSERVER) {
        bind(m_socket, ...);
        listen(m_socket, 32);
    }

    // 设置非阻塞
    if (param.attr & SOCK_ISNONBLOCK) {
        int option = fcntl(m_socket, F_GETFL);
        option |= O_NONBLOCK;
        fcntl(m_socket, F_SETFL, option);
    }
}
```

---

## 六、CSocketBase抽象基类

### 6.1 设计目标

**核心目标**：定义socket操作的统一接口，强制子类实现5个核心函数。

**设计原则**：
1. **接口与实现分离**：基类只定义"做什么"，子类决定"怎么做"
2. **多态**：通过基类指针操作不同类型的socket
3. **强制约束**：纯虚函数保证子类必须实现

---

### 6.2 完整实现

```cpp
class CSocketBase
{
public:
    // 构造函数：初始化成员变量
    CSocketBase() {
        m_socket = -1;   // -1表示无效fd
        m_status = 0;    // 0表示未初始化
    }

    // 虚析构函数：确保通过基类指针删除时调用子类析构
    virtual ~CSocketBase() {
        Close();  // 自动关闭socket
    }

public:
    // ==================== 5个纯虚函数 ====================

    // 初始化socket
    // 服务器：socket() + bind() + listen()
    // 客户端：socket()
    virtual int Init(const CSockParam& param) = 0;

    // 建立连接
    // 服务器：accept()，返回客户端对象
    // 客户端：connect()
    virtual int Link(CSocketBase** pClient = NULL) = 0;

    // 发送数据
    // TCP：send()
    // UDP：sendto()
    virtual int Send(const Buffer& data) = 0;

    // 接收数据
    // TCP：recv()
    // UDP：recvfrom()
    virtual int Recv(Buffer& data) = 0;

    // 关闭socket
    virtual int Close() {
        m_status = 3;  // 标记为已关闭
        if (m_socket != -1) {
            int fd = m_socket;
            m_socket = -1;  // 先设置-1
            close(fd);      // 再关闭
        }
        return 0;
    }

protected:
    // ==================== 成员变量 ====================

    int m_socket;    // socket文件描述符
    int m_status;    // 状态机：0/1/2/3
    CSockParam m_param;  // 初始化参数（018添加）
};
```

---

### 6.3 五个纯虚函数详解

#### 函数1：Init() - 初始化

**职责**：创建socket并完成初始化配置

**不同实现的差异**：

| 实现 | socket()参数 | bind()地址 | listen() |
|------|-------------|-----------|---------|
| CLocalSocket | `(PF_LOCAL, type, 0)` | `sockaddr_un` | TCP需要 |
| CTcpSocket | `(PF_INET, SOCK_STREAM, 0)` | `sockaddr_in` | 需要 |
| CUdpSocket | `(PF_INET, SOCK_DGRAM, 0)` | `sockaddr_in` | 不需要 |

**返回值规范**：
- `0`：成功
- `-1`：状态错误（已初始化）
- `-2`：socket创建失败
- `-3`：bind失败
- `-4`：listen失败
- `-5/-6`：非阻塞设置失败

---

#### 函数2：Link() - 建立连接

**职责**：服务器accept或客户端connect

**参数设计**：
```cpp
virtual int Link(CSocketBase** pClient = NULL) = 0;
//               ^^^^^^^^^^^^^^^^^^^^^^^^
//               双重指针，默认值NULL
```

**为什么用双重指针？**
- 服务器accept后需要"返回"客户端对象
- 单指针无法修改外部变量
- 双重指针可以修改外部指针的指向

**使用方式**：
```cpp
// 服务器
CSocketBase* client = nullptr;
server->Link(&client);  // 传递client的地址
// Link内部：*pClient = new CLocalSocket(client_fd);

// 客户端
client->Link();  // 使用默认值NULL
```

**不同实现的差异**：

| 实现 | 服务器 | 客户端 |
|------|--------|--------|
| CLocalSocket/CTcpSocket | `accept()` | `connect()` |
| CUdpSocket | 返回0（无连接） | 返回0（无连接） |

---

#### 函数3：Send() - 发送数据

**职责**：发送数据到对端

**不同实现的差异**：

| 实现 | 使用的API | 特点 |
|------|----------|------|
| CLocalSocket/CTcpSocket | `send()` / `write()` | 面向连接，不需要指定地址 |
| CUdpSocket | `sendto()` | 无连接，需要指定目标地址 |

**返回值**：
- `> 0`：实际发送的字节数
- `0`：连接关闭
- `< 0`：发送失败

---

#### 函数4：Recv() - 接收数据

**职责**：从对端接收数据

**不同实现的差异**：

| 实现 | 使用的API | 特点 |
|------|----------|------|
| CLocalSocket/CTcpSocket | `recv()` / `read()` | 面向连接 |
| CUdpSocket | `recvfrom()` | 无连接，返回来源地址 |

**返回值**：
- `> 0`：实际接收的字节数
- `0`：连接关闭（TCP）或没收到数据（非阻塞）
- `< 0`：接收失败

---

#### 函数5：Close() - 关闭连接

**职责**：关闭socket，释放资源

**为什么Close不是纯虚函数？**
- 所有socket的关闭逻辑都相同（调用close系统调用）
- 提供默认实现，子类可以直接继承

**实现细节**：
```cpp
virtual int Close() {
    m_status = 3;              // ① 先标记状态
    if (m_socket != -1) {
        int fd = m_socket;     // ② 保存fd
        m_socket = -1;         // ③ 立即设置-1
        close(fd);             // ④ 再关闭
    }
    return 0;
}
```

**为什么先赋值-1再关闭？**
```cpp
// ❌ 错误顺序
close(m_socket);  // 假设这里被信号中断
m_socket = -1;    // 这行可能不执行

// 后果：
// 1. m_socket还是原值
// 2. 析构时重复close
// 3. 可能关闭了别人的fd

// ✅ 正确顺序
int fd = m_socket;
m_socket = -1;    // 立即设置，防止中断
close(fd);        // 即使中断，m_socket已经-1了
```

---

### 6.4 状态机设计

**四个状态**：
```cpp
0 - 未初始化（对象刚创建）
1 - 已初始化（已调用Init，完成bind/listen）
2 - 已连接（已调用Link，可以收发数据）
3 - 已关闭（已调用Close或析构）
```

**状态转换图**：
```
┌─────────────┐
│   创建对象   │
└──────┬──────┘
       ↓
┌─────────────┐
│   状态0     │
│  未初始化   │
└──────┬──────┘
       │ Init()
       ↓
┌─────────────┐
│   状态1     │
│  已初始化   │  ← bind/listen完成
└──────┬──────┘
       │ Link()
       ↓
┌─────────────┐
│   状态2     │
│   已连接    │  ← 可以收发数据
└──────┬──────┘
       │ Close() / ~CSocketBase()
       ↓
┌─────────────┐
│   状态3     │
│   已关闭    │  ← 对象失效
└─────────────┘
```

**状态检查示例**：
```cpp
int Init(const CSockParam& param) {
    if (m_status != 0) return -1;  // 只有状态0才能Init
    // ...
    m_status = 1;
}

int Link(CSocketBase** pClient) {
    if (m_status != 1) return -1;  // 只有状态1才能Link
    // ...
    m_status = 2;
}

int Send(const Buffer& data) {
    if (m_status < 2) return -1;  // 状态2或3才能Send
    // ...
}
```

---

### 6.5 虚析构函数详解

#### 问题：为什么必须是virtual？

**没有virtual的后果**：
```cpp
class CSocketBase {
    ~CSocketBase() { /* 清理基类资源 */ }  // ❌ 非虚析构
};

class CTcpSocket : public CSocketBase {
    ~CTcpSocket() { /* 清理子类资源 */ }
private:
    char* m_buffer;  // 子类特有资源
};

// 使用
CSocketBase* sock = new CTcpSocket();
delete sock;

// 执行结果：
// 1. 只调用 ~CSocketBase()
// 2. 不调用 ~CTcpSocket()
// 3. m_buffer 泄漏！
```

**有virtual的效果**：
```cpp
class CSocketBase {
    virtual ~CSocketBase() { /* 清理基类资源 */ }  // ✅ 虚析构
};

// 使用
CSocketBase* sock = new CTcpSocket();
delete sock;

// 执行结果：
// 1. 先调用 ~CTcpSocket()（清理子类资源）
// 2. 再调用 ~CSocketBase()（清理基类资源）
// 3. 资源全部释放 ✅
```

---

#### 虚析构的工作原理

**虚函数表（vtable）**：
```
CTcpSocket对象内存布局：
┌─────────────┐
│ vptr        │ → ┌──────────────┐
├─────────────┤   │ ~CTcpSocket  │ ← 虚析构函数指针
│ m_socket    │   │ Init         │
│ m_status    │   │ Link         │
│ m_param     │   │ Send         │
│ ...         │   │ Recv         │
└─────────────┘   │ Close        │
                  └──────────────┘

delete sock;  // sock指向CTcpSocket对象
// 1. 查虚函数表找到 ~CTcpSocket
// 2. 调用 ~CTcpSocket()
// 3. 自动调用 ~CSocketBase()
```

---

### 6.6 成员变量详解

```cpp
protected:
    int m_socket;        // socket文件描述符
    int m_status;        // 状态机
    CSockParam m_param;  // 初始化参数（018添加）
```

**为什么是protected而非private？**
- 子类需要访问这些成员
- 不暴露给外部（encapsulation）

**m_param的作用**（018新增）：
```cpp
// Link函数需要用到参数
int Link(CSocketBase** pClient) {
    if (m_param.attr & SOCK_ISSERVER) {
        // 服务器逻辑：accept
    } else {
        // 客户端逻辑：connect
        connect(m_socket, m_param.addrun(), ...);
    }
}
```

---

## 七、CLocalSocket实现

### 7.1 设计概述

**CLocalSocket**：Unix域socket的具体实现，用于本机进程间通信（IPC）。

**特点**：
- 基于文件系统路径通信（不走网络栈）
- 性能高于TCP（无需封包/解包）
- 只能本机通信（无法跨机器）

**使用场景**：
- Nginx master/worker进程通信
- Docker容器与宿主机通信
- X Window系统客户端/服务器通信
- systemd守护进程通信

---

### 7.2 类定义

```cpp
class CLocalSocket : public CSocketBase
{
public:
    // 默认构造函数
    CLocalSocket() : CSocketBase() {}

    // 特殊构造函数：用于accept返回的客户端
    // 参数sock：已创建的socket fd
    CLocalSocket(int sock) : CSocketBase() {
        m_socket = sock;  // 直接设置fd，Init时不再创建
    }

    // 虚析构函数
    virtual ~CLocalSocket() {
        Close();
    }

    // 实现5个纯虚函数
    virtual int Init(const CSockParam& param) override;
    virtual int Link(CSocketBase** pClient = NULL) override;
    virtual int Send(const Buffer& data) override;
    virtual int Recv(Buffer& data) override;
    virtual int Close() override;
};
```

---

### 7.3 Init()函数实现详解

```cpp
int CLocalSocket::Init(const CSockParam& param) {
    // ==================== 步骤① 状态检查 ====================
    if (m_status != 0) return -1;
    // 防止重复初始化，避免fd泄漏

    // ==================== 步骤② 保存参数 ====================
    m_param = param;
    // Link/Send/Recv会用到

    // ==================== 步骤③ 选择socket类型 ====================
    int type = (m_param.attr & SOCK_ISUDP) ? SOCK_DGRAM : SOCK_STREAM;
    //         ^^^^^^^^^^^^^^^^^^^^^^^^^^^   ^^^^^^^^^^^   ^^^^^^^^^^^
    //         检查UDP标志                   UDP类型       TCP类型

    // ==================== 步骤④ 创建socket ====================
    if (m_socket == -1)  // 先检查是否已创建
        m_socket = socket(PF_LOCAL, type, 0);
    //                   ^^^^^^^^  ^^^^  ^
    //                   Unix域    类型  协议（0=自动选择）

    if (m_socket == -1) return -2;

    int ret = 0;

    // ==================== 步骤⑤ 服务器：bind + listen ====================
    if (m_param.attr & SOCK_ISSERVER) {
        // bind：绑定Unix域socket到文件路径
        ret = bind(m_socket, m_param.addrun(), sizeof(sockaddr_un));
        //                   ^^^^^^^^^^^^^^^   ^^^^^^^^^^^^^^^^^^
        //                   Unix域地址指针     地址结构大小
        if (ret == -1) return -3;

        // listen：开始监听（TCP模式）
        ret = listen(m_socket, 32);
        //                     ^^
        //                     backlog：等待队列长度
        if (ret == -1) return -4;
    }
    // 客户端不需要bind/listen

    // ==================== 步骤⑥ 设置非阻塞 ====================
    if (m_param.attr & SOCK_ISNONBLOCK) {
        // 获取当前文件状态标志
        int option = fcntl(m_socket, F_GETFL);
        if (option == -1) return -5;

        // 添加O_NONBLOCK标志
        option |= O_NONBLOCK;

        // 设置新的文件状态标志
        ret = fcntl(m_socket, F_SETFL, option);
        if (ret == -1) return -6;
    }

    // ==================== 步骤⑦ 更新状态 ====================
    m_status = 1;  // 已初始化
    return 0;
}
```

---

### 7.4 Init()的7个步骤详解

#### 步骤①：状态检查

**目的**：防止重复初始化

```cpp
if (m_status != 0) return -1;

// 场景：
CLocalSocket sock;
sock.Init(param1);  // ✅ m_status=0，允许
sock.Init(param2);  // ❌ m_status=1，拒绝

// 如果不检查：
sock.Init(param1);  // 创建fd=3
sock.Init(param2);  // 又创建fd=4，fd=3泄漏！
```

---

#### 步骤②：保存参数

**目的**：后续函数需要用到参数

```cpp
m_param = param;

// 使用场景：
// Link()函数需要判断是服务器还是客户端
if (m_param.attr & SOCK_ISSERVER) {
    // accept逻辑
} else {
    // connect逻辑
    connect(m_socket, m_param.addrun(), ...);
}
```

---

#### 步骤③：选择socket类型

**目的**：根据UDP标志选择SOCK_STREAM或SOCK_DGRAM

```cpp
int type = (m_param.attr & SOCK_ISUDP) ? SOCK_DGRAM : SOCK_STREAM;

// 示例：
// TCP服务器
CSockParam param1("/tmp/tcp.sock", SOCK_ISSERVER);
// attr = 0001, type = SOCK_STREAM

// UDP服务器
CSockParam param2("/tmp/udp.sock", SOCK_ISSERVER | SOCK_ISUDP);
// attr = 0101, type = SOCK_DGRAM
```

**socket类型的区别**：

| 类型 | 特点 | 使用场景 |
|------|------|---------|
| SOCK_STREAM | 面向连接、可靠、有序 | TCP、Unix域TCP |
| SOCK_DGRAM | 无连接、不可靠、无序 | UDP、Unix域UDP |

---

#### 步骤④：创建socket

**目的**：调用socket系统调用创建文件描述符

```cpp
if (m_socket == -1)
    m_socket = socket(PF_LOCAL, type, 0);
```

**为什么先判断m_socket == -1？**
- accept返回的客户端socket已经有fd了
- 通过`CLocalSocket(int sock)`构造时，m_socket已设置
- 避免重复创建

**socket()函数原型**：
```cpp
int socket(int domain, int type, int protocol);
//         ^^^^^^^^^   ^^^^^^^^   ^^^^^^^^^^^
//         协议族      类型       协议

// PF_LOCAL = AF_UNIX = 1（Unix域）
// PF_INET = AF_INET = 2（IPv4）
// PF_INET6 = AF_INET6 = 10（IPv6）
```

---

#### 步骤⑤：bind + listen

**目的**：服务器绑定地址并开始监听

```cpp
if (m_param.attr & SOCK_ISSERVER) {
    // bind
    ret = bind(m_socket, m_param.addrun(), sizeof(sockaddr_un));
    if (ret == -1) return -3;

    // listen
    ret = listen(m_socket, 32);
    if (ret == -1) return -4;
}
```

**bind()详解**：
```cpp
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
//       ^^^^^^^^^^  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^  ^^^^^^^^^^^^^^^^
//       socket fd   地址结构指针                   地址结构大小

// Unix域socket bind后会创建socket文件
bind(..., "/tmp/server.sock", ...);
// 执行后：/tmp/server.sock 文件被创建
// 类型：socket文件（ls -l 显示为 s开头）
```

**listen()详解**：
```cpp
int listen(int sockfd, int backlog);
//         ^^^^^^^^^^  ^^^^^^^^^^^
//         socket fd   等待队列长度

// backlog：
// - 同时等待accept的连接数
// - 超过backlog的连接会被拒绝
// - 常用值：32, 128, 511
```

---

#### 步骤⑥：设置非阻塞

**目的**：配置socket为非阻塞模式

```cpp
if (m_param.attr & SOCK_ISNONBLOCK) {
    int option = fcntl(m_socket, F_GETFL);  // 获取标志
    option |= O_NONBLOCK;                   // 添加非阻塞标志
    fcntl(m_socket, F_SETFL, option);       // 设置标志
}
```

**阻塞 vs 非阻塞**：

| 模式 | accept() | recv() | 适用场景 |
|------|---------|--------|---------|
| **阻塞** | 等待连接，阻塞 | 等待数据，阻塞 | 简单同步服务器 |
| **非阻塞** | 无连接立即返回-1 | 无数据立即返回-1 | Epoll + 事件循环 |

**fcntl()详解**：
```cpp
// 获取文件状态标志
int option = fcntl(fd, F_GETFL);
// option 可能包含：O_RDONLY, O_WRONLY, O_RDWR, O_NONBLOCK等

// 添加非阻塞标志（位运算）
option |= O_NONBLOCK;
//  假设option = 0010
//  O_NONBLOCK = 0100
//  option |= 0100 → option = 0110

// 设置文件状态标志
fcntl(fd, F_SETFL, option);
```

---

#### 步骤⑦：更新状态

**目的**：标记socket已初始化

```cpp
m_status = 1;  // 0（未初始化）→ 1（已初始化）
return 0;      // 成功
```

---

### 7.5 Init()错误码总结

| 返回值 | 含义 | 可能原因 |
|--------|------|---------|
| 0 | 成功 | - |
| -1 | 状态错误 | 已经初始化过 |
| -2 | socket创建失败 | 文件描述符用尽、权限不足 |
| -3 | bind失败 | socket文件已存在、路径不存在、权限不足 |
| -4 | listen失败 | socket未bind、UDP不支持listen |
| -5 | fcntl获取失败 | 无效的fd |
| -6 | fcntl设置失败 | 无效的标志 |

---

### 7.6 测试Init()函数

```cpp
#include "Socket.h"
#include <cstdio>

int main() {
    printf("=== 测试CLocalSocket::Init() ===\n\n");

    // 测试1：创建服务器socket
    printf("测试1：创建服务器socket\n");
    unlink("/tmp/test_server.sock");  // 删除旧文件

    CSockParam server_param("/tmp/test_server.sock", SOCK_ISSERVER);
    CLocalSocket server;

    int ret = server.Init(server_param);
    if (ret == 0) {
        printf("✅ 服务器初始化成功\n");
    } else {
        printf("❌ 服务器初始化失败，错误码: %d\n", ret);
        if (ret == -3) printf("   提示：可能是socket文件已存在\n");
        return -1;
    }

    // 测试2：创建客户端socket
    printf("\n测试2：创建客户端socket\n");
    CSockParam client_param("/tmp/test_client.sock", 0);
    CLocalSocket client;

    ret = client.Init(client_param);
    if (ret == 0) {
        printf("✅ 客户端初始化成功\n");
    } else {
        printf("❌ 客户端初始化失败，错误码: %d\n", ret);
        return -1;
    }

    // 测试3：防止重复初始化
    printf("\n测试3：防止重复初始化\n");
    ret = server.Init(server_param);
    if (ret == -1) {
        printf("✅ 正确拒绝重复初始化\n");
    } else {
        printf("❌ 应该拒绝重复初始化！\n");
    }

    // 测试4：非阻塞模式
    printf("\n测试4：非阻塞模式\n");
    unlink("/tmp/test_nonblock.sock");
    CSockParam nb_param("/tmp/test_nonblock.sock",
                        SOCK_ISSERVER | SOCK_ISNONBLOCK);
    CLocalSocket nb_server;

    ret = nb_server.Init(nb_param);
    if (ret == 0) {
        printf("✅ 非阻塞服务器初始化成功\n");
    } else {
        printf("❌ 非阻塞服务器初始化失败，错误码: %d\n", ret);
    }

    // 清理
    unlink("/tmp/test_server.sock");
    unlink("/tmp/test_client.sock");
    unlink("/tmp/test_nonblock.sock");

    printf("\n=== Init()测试完成 ===\n");
    return 0;
}
```

**预期输出**：
```
=== 测试CLocalSocket::Init() ===

测试1：创建服务器socket
✅ 服务器初始化成功

测试2：创建客户端socket
✅ 客户端初始化成功

测试3：防止重复初始化
✅ 正确拒绝重复初始化

测试4：非阻塞模式
✅ 非阻塞服务器初始化成功

=== Init()测试完成 ===
```

---

## 八、设计模式应用

### 8.1 策略模式（Strategy Pattern）

**定义**：定义一系列算法，把它们封装起来，并且使它们可以互相替换。

**应用**：
```
策略接口：CSocketBase
    ↓
具体策略：CLocalSocket、CTcpSocket、CUdpSocket
```

**代码示例**：
```cpp
// 上层代码不关心具体实现
void SendData(CSocketBase* sock, const Buffer& data) {
    sock->Send(data);  // 多态调用
    // CLocalSocket::Send() 或 CTcpSocket::Send()？
    // 运行时决定，编译时不知道
}

// 使用
CSocketBase* sock1 = new CLocalSocket();
CSocketBase* sock2 = new CTcpSocket();
SendData(sock1, "hello");  // 调用CLocalSocket::Send()
SendData(sock2, "world");  // 调用CTcpSocket::Send()
```

**优点**：
- 上层代码不依赖具体实现
- 易于扩展（添加新协议）
- 易于测试（mock接口）

---

### 8.2 工厂模式（Factory Pattern）

**当前缺失，应该添加**：

```cpp
class CSocketFactory {
public:
    enum SocketType {
        LOCAL_SOCKET,
        TCP_SOCKET,
        UDP_SOCKET
    };

    static CSocketBase* CreateSocket(SocketType type) {
        switch (type) {
            case LOCAL_SOCKET: return new CLocalSocket();
            case TCP_SOCKET:   return new CTcpSocket();
            case UDP_SOCKET:   return new CUdpSocket();
            default: return nullptr;
        }
    }
};

// 使用
CSocketBase* sock = CSocketFactory::CreateSocket(
    CSocketFactory::TCP_SOCKET
);
sock->Init(param);
```

---

### 8.3 RAII（资源获取即初始化）

**定义**：构造时获取资源，析构时释放资源。

**应用1：Buffer类**
```cpp
Buffer buf(1024);  // 构造：分配内存
// ... 使用
// 析构：自动释放（std::string的析构）
```

**应用2：CSocketBase类**
```cpp
CLocalSocket sock;
sock.Init(param);  // 创建socket fd
// ... 使用
// 析构：自动close(fd)
```

**优点**：
- 防止资源泄漏
- 异常安全（栈展开时自动析构）
- 代码简洁（不需要手动释放）

---

### 8.4 适配器模式（Adapter Pattern）

**定义**：将一个类的接口转换成客户期望的另一个接口。

**应用：Buffer类**
```
std::string（已有接口）
    ↓
Buffer（适配器）
    ↓
char*（socket API需要的接口）
```

**代码示例**：
```cpp
// std::string无法直接用于socket
std::string data = "hello";
send(fd, data, data.size(), 0);  // ❌ 编译错误

// Buffer适配后
Buffer data = "hello";
send(fd, data, data.size(), 0);  // ✅ 自动转换
```

---

### 8.5 模板方法模式（Template Method Pattern）

**定义**：定义算法骨架，把某些步骤延迟到子类实现。

**应用：CSocketBase定义流程**
```cpp
// 固定的使用流程（模板方法）
int UseSocket(CSocketBase* sock, const CSockParam& param) {
    sock->Init(param);    // 步骤1
    sock->Link();         // 步骤2
    sock->Send(data);     // 步骤3
    sock->Recv(data);     // 步骤4
    sock->Close();        // 步骤5
    // 流程固定，但每步的具体实现由子类决定
}
```

---

## 九、关键问题与答案

### Q1：为什么Buffer需要三个版本的转换运算符？

**A**：因为需要适配不同的使用场景，编译器根据对象类型和目标类型选择合适的版本。

| 版本 | 对象 | 目标 | 场景 |
|------|------|------|------|
| `operator char*()` | 非const | char* | recv写入 |
| `operator char*() const` | const | char* | 老API |
| `operator const char*() const` | const | const char* | printf只读 |

**如果少一个版本**：某些场景会编译错误或类型不匹配。

---

### Q2：为什么CSockParam需要两个地址结构体？

**A**：因为TCP/UDP和Unix域socket的地址结构完全不同，无法合并。

```cpp
sockaddr_in：存储 IP + 端口（16字节）
sockaddr_un：存储 文件路径（110字节）
```

**设计**：同时存储两个，根据构造函数决定用哪个。

---

### Q3：为什么Unix域用strcpy而TCP用直接赋值？

**A**：因为数据类型不同。

```cpp
// TCP的IP：uint32_t整数
addr_in.sin_addr.s_addr = inet_addr(ip);  // 整数赋值

// Unix域路径：char[108]数组
strcpy(addr_un.sun_path, path);  // 数组复制
```

**数组不能直接赋值**：
```cpp
char arr[10];
arr = "hello";  // ❌ 错误：数组名是地址常量
strcpy(arr, "hello");  // ✅ 正确：复制内容
```

---

### Q4：SockAttr为什么用位标志？

**A**：位标志可以组合多个属性，简洁高效。

```cpp
// 优点：
int attr = SOCK_ISSERVER | SOCK_ISNONBLOCK | SOCK_ISUDP;  // 一个参数
if (attr & SOCK_ISSERVER) { /* 检查 */ }  // 位运算高效

// 缺点：
CSockParam(ip, port, bool is_server, bool is_nonblock, bool is_udp);  // 3个参数
```

**值必须是2的幂次**：确保每个标志占用独立的bit位。

---

### Q5：为什么CSocketBase需要虚析构函数？

**A**：防止通过基类指针删除子类对象时，只调用基类析构导致资源泄漏。

```cpp
CSocketBase* sock = new CTcpSocket();
delete sock;

// 没有virtual：只调用~CSocketBase()，子类资源泄漏
// 有virtual：先调用~CTcpSocket()，再调用~CSocketBase()
```

**规则**：有虚函数的类必须有虚析构。

---

### Q6：Link的参数为什么是双重指针？

**A**：因为需要"返回"客户端对象，单指针无法修改外部变量。

```cpp
// 单指针：无法修改外部变量
void Link(CSocketBase* p) {
    p = new CTcpSocket();  // 只修改局部副本
}

// 双重指针：可以修改外部变量
void Link(CSocketBase** p) {
    *p = new CTcpSocket();  // 修改外部指针的指向
}

// 使用
CSocketBase* client = nullptr;
server->Link(&client);  // 传递client的地址
```

---

### Q7：Link能用引用代替双重指针吗？

**A**：可以，但需要函数重载。

```cpp
// 方案1：引用+重载
virtual int Accept(CSocketBase*& pClient);  // 服务器
virtual int Connect();  // 客户端

// 方案2：双重指针+默认值（参考代码）
virtual int Link(CSocketBase** pClient = NULL);
```

**参考代码选择双重指针的原因**：
1. 一个函数搞定服务器和客户端
2. 用NULL区分服务器/客户端
3. 兼容C++98

---

### Q8：为什么状态机很重要？

**A**：防止误操作，保证操作顺序。

```cpp
// 没有状态机的问题：
sock.Init(param);
sock.Init(param);  // ❌ 重复初始化，fd泄漏
sock.Send(data);   // ❌ 还没连接就发送，崩溃

// 有状态机的保护：
sock.Init(param);       // m_status: 0 → 1
sock.Init(param);       // ❌ m_status != 0，返回-1
sock.Link();            // m_status: 1 → 2
sock.Send(data);        // ✅ m_status == 2，允许
```

---

### Q9：为什么Init()要先设置m_socket=-1再close？

**A**：防止信号中断导致重复关闭。

```cpp
// ❌ 错误顺序
close(m_socket);  // 假设这里被信号中断
m_socket = -1;    // 这行可能不执行

// ✅ 正确顺序
int fd = m_socket;
m_socket = -1;    // 立即设置，防止中断
close(fd);        // 即使中断，m_socket已经-1了
```

---

## 十、学习进度与计划

### 10.1 当前进度

#### 已完成 ✅

1. **Buffer类**
   - 继承std::string
   - 三个转换运算符
   - 自动内存管理

2. **SockAttr枚举**
   - 位标志设计
   - 三个属性：ISSERVER/ISNONBLOCK/ISUDP

3. **CSockParam类**
   - 两个地址结构体
   - 三个构造函数
   - 拷贝构造和赋值运算符

4. **CSocketBase抽象基类**
   - 5个纯虚函数
   - 状态机设计
   - 虚析构函数

5. **CLocalSocket类**
   - 类框架和构造函数
   - Init()函数完整实现（7步）

---

### 10.2 待完成 ⏳

#### 第6步：实现Link()函数
- accept()逻辑（服务器）
- connect()逻辑（客户端）
- 创建客户端对象

#### 第7步：实现Send()和Recv()函数
- write()/send()发送数据
- read()/recv()接收数据
- 错误处理（EINTR/EAGAIN）

#### 第8步：实现Close()函数
- 调用基类Close()
- 或自定义清理逻辑

#### 第9步：完整测试
- 服务器/客户端通信测试
- 非阻塞模式测试
- 错误处理测试

#### 第10步：实现CTcpSocket
- 基于CLocalSocket改写
- 只需修改3处（PF_INET, addrin(), sockaddr_in）

#### 第11步：实现CUdpSocket
- 基于CTcpSocket改写
- 重点：Link()空实现、sendto()、recvfrom()

#### 第12步：整合应用
- Socket + Epoll 单进程服务器
- Socket + Epoll + Process 多进程服务器

---

### 10.3 学习时间估算

| 任务 | 预计时间 | 对话次数 |
|------|---------|---------|
| Link()实现 | 30分钟 | 1-2次 |
| Send/Recv实现 | 30分钟 | 1次 |
| Close实现 | 10分钟 | 1次 |
| 完整测试 | 20分钟 | 1次 |
| CTcpSocket | 20分钟 | 1次 |
| CUdpSocket | 40分钟 | 1-2次 |
| 整合应用 | 60分钟 | 2-3次 |
| **总计** | **3.5小时** | **8-12次对话** |

---

### 10.4 文件清单

**当前Socket.h应该包含**：
```cpp
// 1. 头文件include
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string>
#include <cstring>
#include <cerrno>

// 2. Buffer类
class Buffer : public std::string { ... };

// 3. SockAttr枚举
enum SockAttr { ... };

// 4. CSockParam类
class CSockParam { ... };

// 5. CSocketBase抽象基类
class CSocketBase { ... };

// 6. CLocalSocket类声明
class CLocalSocket : public CSocketBase { ... };

// 7. CLocalSocket::Init()实现
int CLocalSocket::Init(const CSockParam& param) { ... }
```

---

### 10.5 下次学习内容

**标题**：实现CLocalSocket::Link()函数

**内容**：
1. Link()函数的两种逻辑（服务器/客户端）
2. accept()详解
3. connect()详解
4. 客户端对象的创建
5. 测试服务器/客户端连接

**准备工作**：
- 确保Init()函数已实现并测试通过
- 理解accept()和connect()的区别

---

## 十一、参考资料

### 11.1 官方文档

- Linux Man Pages：`man socket`, `man bind`, `man listen`, `man accept`, `man connect`
- POSIX标准：Open Group Base Specifications

### 11.2 推荐书籍

- 《Unix网络编程 卷1：套接字联网API》（第3版）- W. Richard Stevens
- 《Linux高性能服务器编程》- 游双
- 《深入理解Linux网络技术内幕》

### 11.3 系统调用

| 函数 | 头文件 | 作用 |
|------|--------|------|
| `socket()` | `<sys/socket.h>` | 创建socket |
| `bind()` | `<sys/socket.h>` | 绑定地址 |
| `listen()` | `<sys/socket.h>` | 监听连接 |
| `accept()` | `<sys/socket.h>` | 接受连接 |
| `connect()` | `<sys/socket.h>` | 连接服务器 |
| `send()/recv()` | `<sys/socket.h>` | 发送/接收数据 |
| `close()` | `<unistd.h>` | 关闭fd |
| `fcntl()` | `<fcntl.h>` | 文件控制 |
| `inet_addr()` | `<arpa/inet.h>` | IP字符串→整数 |
| `htons()` | `<arpa/inet.h>` | 主机序→网络序 |

---

## 十二、总结

### 12.1 学习重点回顾

**核心概念**：
1. **分层架构**：工具层 → 接口层 → 实现层
2. **策略模式**：统一接口，可替换实现
3. **RAII**：资源自动管理
4. **状态机**：防止误操作

**关键技术**：
1. **Buffer**：继承std::string + char*转换
2. **CSockParam**：两个地址结构体 + 三个构造函数
3. **SockAttr**：位标志设计
4. **CSocketBase**：5个纯虚函数 + 虚析构
5. **CLocalSocket**：Init()的7步实现

---

### 12.2 设计亮点

1. **统一接口**：TCP/UDP/Unix域用同一套API
2. **类型安全**：Buffer的三个转换运算符
3. **易于扩展**：添加新协议不影响现有代码
4. **自动管理**：析构时自动关闭socket
5. **灵活配置**：位标志组合多个属性

---

### 12.3 后续展望

**短期目标**（1-2天）：
- 完成CLocalSocket的所有函数
- 实现CTcpSocket和CUdpSocket
- 编写完整的测试程序

**中期目标**（1周）：
- 整合Socket + Epoll
- 编写单进程高性能服务器
- 整合Socket + Epoll + Process
- 编写多进程负载均衡服务器

**长期目标**（1个月）：
- 实现完整的易播服务器
- 支持HTTP协议
- 支持流媒体传输
- 性能优化和压力测试

---

**学习日期**：2025-11-20
**项目状态**：CLocalSocket::Init()实现完成 ✅
**下一步**：实现Link()函数，完成accept和connect逻辑
**预计完成时间**：3-4小时（8-12次对话）

---

**更新日期**：2025-11-20
**文档版本**：v2.0（详细版）
**字数统计**：约2.5万字
**适用对象**：C++ Linux网络编程学习者
