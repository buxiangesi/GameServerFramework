#pragma once
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <cstring>
#include <fcntl.h>  

class Buffer : public std::string
{
public:
    Buffer() : std::string() {}
    Buffer(size_t size) : std::string() { resize(size); }
    // ⭐ 添加这个构造函数：从const char*构造
    Buffer(const char* str) : std::string(str) {}

    // ⭐ 添加这个构造函数：从std::string构造
    Buffer(const std::string& str) : std::string(str) {}
    operator char* () { return (char*)c_str(); }
    operator char* () const { return (char*)c_str(); }
    operator const char* () const { return c_str(); }
};
// ============================================
// SockAttr枚举：Socket属性标志
// 使用位标志，可以用 | 运算符组合
// ============================================
enum SockAttr {
    SOCK_ISSERVER = 1,  // 0001：是否是服务器（1=服务器，0=客户端）
    SOCK_ISBLOCK = 2,   // 0010：是否阻塞（1=阻塞，0=非阻塞）
    // 未来可扩展：
    // SOCK_ISREUSE = 4,   // 0100：是否复用地址
    // SOCK_ISKEEPALIVE = 8 // 1000：是否保持连接
};

// ============================================
// CSockParam类：Socket参数封装
// 统一管理TCP/UDP/Unix域socket的参数
// ============================================
class CSockParam {
public:
    // -------------------- 构造函数 --------------------

    // 默认构造函数：创建空参数对象
    CSockParam() {
        bzero(&addr_in, sizeof(addr_in));  // 清零TCP/UDP地址
        bzero(&addr_un, sizeof(addr_un));  // 清零Unix域地址
        port = -1;   // -1表示无效端口
        attr = 0;    // 无属性
    }
    //使用场景

    // 场景1：先创建，后赋值
    //  CSockParam param;  // 默认构造
    // ... 从配置文件读取
    // param.ip = config.get("ip");
    //param.port = config.get("port");

    // 场景2：数组初始化
    //CSockParam params[10];  // 10个空参数对象

    // TCP/UDP构造函数
    // 参数：ip - IP地址（如"0.0.0.0"、"192.168.1.1"）
    //      port - 端口号（1-65535）
    //      attr - 属性标志（SOCK_ISSERVER | SOCK_ISBLOCK）
    CSockParam(const Buffer& ip, short port, int attr) {
        this->ip = ip;
        this->port = port;
        this->attr = attr;

        // 初始化TCP/UDP地址结构体
        addr_in.sin_family = AF_INET;              // 地址族：IPv4
        addr_in.sin_port = htons(port);  // host to network short
        //端口（注意：实际应用应用htons转换）
        addr_in.sin_addr.s_addr = inet_addr(ip);   // IP地址字符串→网络字节序
    }

    // Unix域socket构造函数
    // 参数：path - Unix域socket文件路径（如"/tmp/server.sock"）
    //      attr - 属性标志
    CSockParam(const Buffer& path, int attr) {
        ip = path;  // 复用ip字段存储路径

        // 初始化Unix域地址结构体
        addr_un.sun_family = AF_UNIX;        // 地址族：Unix域
        // ✅ 安全版本：限制最大长度
        size_t max_len = sizeof(addr_un.sun_path) - 1;  // 预留1字节给'\0'
        strncpy(addr_un.sun_path, path, max_len);
        addr_un.sun_path[max_len] = '\0';  // 确保结尾

        this->attr = attr;
    }

    // 析构函数
    ~CSockParam() {}

    // -------------------- 拷贝构造/赋值 --------------------

    // 拷贝构造函数：CSockParam p2 = p1;
    CSockParam(const CSockParam& param) {
        ip = param.ip;
        port = param.port;
        attr = param.attr;
        memcpy(&addr_in, &param.addr_in, sizeof(addr_in));
        memcpy(&addr_un, &param.addr_un, sizeof(addr_un));
    }

    // 赋值运算符：p2 = p1;
    CSockParam& operator=(const CSockParam& param) {
        if (this != &param) {  // 防止自赋值
            ip = param.ip;
            port = param.port;
            attr = param.attr;
            memcpy(&addr_in, &param.addr_in, sizeof(addr_in));
            memcpy(&addr_un, &param.addr_un, sizeof(addr_un));
        }
        return *this;
    }

    // -------------------- 辅助方法 --------------------

    // 获取TCP/UDP地址指针（类型转换为sockaddr*）
    // 用途：bind(fd, param.addrin(), sizeof(sockaddr_in))
    sockaddr* addrin() const { return (sockaddr*)&addr_in; }

    // 获取Unix域地址指针（类型转换为sockaddr*）
    // 用途：bind(fd, param.addrun(), sizeof(sockaddr_un))
    sockaddr* addrun() const { return (sockaddr*)&addr_un; }

    // -------------------- 成员变量 --------------------
public:
    sockaddr_in addr_in;   // TCP/UDP地址结构体
    sockaddr_un addr_un;   // Unix域地址结构体
    Buffer ip;             // IP地址或Unix路径
    short port;            // 端口号（-1表示无效）
    int attr;              // 属性标志（SockAttr枚举的组合）
};

// ============================================
// CSocketBase类：Socket抽象基类
// 定义统一的Socket接口，子类实现具体协议
// ============================================
class CSocketBase
{
public:
    // ========== 添加构造函数 ==========
    CSocketBase() {
        m_socket = -1;  // 初始化为无效描述符
        m_status = 0;   // 初始化为未初始化状态
    }

    // -------------------- 虚析构函数 --------------------

    // 虚析构函数：确保子类对象通过基类指针删除时正确析构
    virtual ~CSocketBase() {
        m_status = 3;  // 标记为已关闭

        // 关闭socket文件描述符
        if (m_socket != -1) {
            int fd = m_socket;
            m_socket = -1;      // 先设置-1，防止重复关闭
            close(fd);          // 再关闭fd
        }
    }

    // -------------------- 纯虚函数（子类必须实现） --------------------

    // 初始化socket
    // 服务器：创建socket → bind → listen
    // 客户端：创建socket
    // 返回值：0成功，负数失败
    virtual int Init(const CSockParam& param) = 0;

    // 建立连接
    // 服务器：accept等待客户端连接，pClient返回客户端socket对象
    // 客户端：connect连接到服务器
    // UDP：不需要连接（但可能需要实现空操作）
    // 返回值：0成功，负数失败
    virtual int Link(CSocketBase** pClient = NULL) = 0;

    // 发送数据
    // TCP：send()
    // UDP：sendto()
    // 返回值：发送的字节数，负数失败
    virtual int Send(const Buffer& data) = 0;

    // 接收数据
    // TCP：recv()
    // UDP：recvfrom()
    // 返回值：接收的字节数，0表示连接关闭，负数失败
    virtual int Recv(Buffer& data) = 0;

    // 关闭连接
    // 返回值：0成功，负数失败
    virtual int Close() = 0;

protected:
    // -------------------- 成员变量 --------------------

    // socket文件描述符，默认-1（无效）
    int m_socket;

    // socket状态机
    // 0 - 未初始化
    // 1 - 已初始化（已调用Init）
    // 2 - 已连接（已调用Link）
    // 3 - 已关闭（已调用Close或析构）
    int m_status;
};

  //二、为什么需要抽象基类？

  //问题：TCP、UDP、Unix域socket差异太大

  //// TCP服务器流程
  //socket() → bind() → listen() → accept() → send()/recv()

  //// UDP流程（没有连接！）
  //socket() → bind() → sendto()/recvfrom()

  //// Unix域
  //socket(AF_UNIX) → bind("/path") → listen() → accept() → ...

  //三种协议的操作步骤完全不同！
class CLocalSocket :public CSocketBase {
    // -------------------- 构造函数 --------------------
public:
     // 默认构造函数
    CLocalSocket() : CSocketBase() {}

    // 特殊构造函数：用于accept返回的客户端socket
    // 参数sock：已经创建好的socket文件描述符
    CLocalSocket(int sock) : CSocketBase() {
        m_socket = sock;
    }

    // 虚析构函数
    virtual ~CLocalSocket() {
        Close();
    }

    // -------------------- 纯虚函数实现 --------------------

    // 初始化socket（创建、绑定、监听）
    virtual int Init(const CSockParam& param) override;

    // 建立连接（服务器accept，客户端connect）
    virtual int Link(CSocketBase** pClient = NULL) override;

    // 发送数据
    virtual int Send(const Buffer& data) override;

    // 接收数据
    virtual int Recv(Buffer& data) override;

    // 关闭socket
    virtual int Close() override;
private:
    CSockParam m_param;
};


class CTcpSocket : public CSocketBase {
public:
    // ========== 构造函数（和CLocalSocket一样） ==========
    CTcpSocket() : CSocketBase() {}
    CTcpSocket(int sock) : CSocketBase() { m_socket = sock; }
    virtual ~CTcpSocket() { Close(); }

    // ========== 纯虚函数实现 ==========
    virtual int Init(const CSockParam& param) override;
    virtual int Link(CSocketBase** pClient = NULL) override;
    virtual int Send(const Buffer& data) override;
    virtual int Recv(Buffer& data) override;
    virtual int Close() override;

protected:
    CSockParam m_param;  // 保存参数
};

class CUdpSocket : public CSocketBase {
public:
    CUdpSocket() : CSocketBase() {}
    virtual ~CUdpSocket() { Close(); }

    virtual int Init(const CSockParam& param) override;
    virtual int Link(CSocketBase** pClient = NULL) override;
    virtual int Send(const Buffer& data) override;
    virtual int Recv(Buffer& data) override;
    virtual int Close() override;

protected:
    CSockParam m_param;
};

class Socket
{
};

