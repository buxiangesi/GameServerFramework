#include "Socket.h"

int CLocalSocket::Init(const CSockParam& param)
{
    // 第1步：状态检查
    if (m_status != 0) return -1;

    // 第2步：保存参数
    m_param = param;

    // 第3步：选择类型
    int type = SOCK_STREAM;

    // 第4步：创建socket
    m_socket = socket(AF_UNIX, type, 0);
    if (m_socket == -1) return -2;

    // 第5步：服务器绑定+监听
    if (param.attr & SOCK_ISSERVER) {
        unlink(param.ip);  // 先删除旧文件
        if (bind(m_socket, param.addrun(), sizeof(sockaddr_un)) == -1) {
            return -3;
        }
        if (listen(m_socket, 10) == -1) {
            return -4;
        }
    }

    // 第6步：设置非阻塞
    if (param.attr & SOCK_ISBLOCK) {
        int flags = fcntl(m_socket, F_GETFL, 0);
        fcntl(m_socket, F_SETFL, flags | O_NONBLOCK);
    }

    //int fcntl(int fd, int cmd, ... /* arg */);

    //三个参数：
    //    - fd：文件描述符（socket也是文件描述符）
    //    - cmd：命令（要做什么操作）
    //    - arg：参数（可选，根据cmd决定）


    // 第7步：更新状态
    m_status = 1;
    return 0;
}
int CLocalSocket::Link(CSocketBase** pClient) {
    // ========== 第1步：状态检查 ==========
    if (m_status != 1) {
        return -1;  // 必须先调用Init()
    }

    // ========== 第2步：判断服务器/客户端 ==========
    if (m_param.attr & SOCK_ISSERVER) {
        // ==================== 服务器逻辑 ====================

        // 检查：服务器必须传pClient参数
        if (pClient == NULL) {
            return -2;  // 服务器必须提供pClient接收客户端对象
        }

        // accept等待客户端连接
        sockaddr_un client_addr;
        socklen_t len = sizeof(client_addr);
        int client_fd = accept(m_socket, (sockaddr*)&client_addr, &len);

        if (client_fd == -1) {
            return -3;  // accept失败
        }

        // 创建客户端socket对象（用特殊构造函数）
        CLocalSocket* new_client = new CLocalSocket(client_fd);
        new_client->m_status = 2;  // 设置为已连接状态

        // ✅ 通过双重指针返回客户端对象
        *pClient = new_client;
        //↑↑↑↑↑↑↑↑
        // 修改外部的client指针，让它指向new_client

        // 服务器本身状态不变（还是1-已初始化）
        // 因为可以继续accept其他客户端

        return 0;  // 成功

    }
    else {
        // ==================== 客户端逻辑 ====================

        // connect连接到服务器
        if (connect(m_socket, m_param.addrun(), sizeof(sockaddr_un)) == -1) {
            return -4;  // connect失败
        }

        // 更新状态为已连接
        m_status = 2;

        return 0;  // 成功
    }
}
int CLocalSocket::Send(const Buffer& data) {
    // 第1步：状态检查
    if (m_status != 2) {
        return -1;  // 必须已连接
    }

    // 第2步：发送数据
    int ret = send(m_socket, (const char*)data.c_str(), data.size(), 0);

    // 第3步：返回结果
    return ret;  // 返回发送的字节数，-1表示失败
}

int CLocalSocket::Recv(Buffer& data) {
    // 第1步：状态检查
    if (m_status != 2) {
        return -1;  // 必须已连接
    }

    // 第2步：接收数据
    int ret = recv(m_socket, (char*)data.c_str(), data.size(), 0);

    // 第3步：返回结果
    return ret;  // >0:接收字节数, 0:连接关闭, -1:失败
}
int CLocalSocket::Close() {
    // 第1步：状态检查
    if (m_status == 0 || m_status == 3) {
        return -1;  // 未初始化或已关闭
    }

    // 第2步：关闭socket
    if (m_socket != -1) {
        int fd = m_socket;
        m_socket = -1;      // 先设为-1
        close(fd);          // 再关闭
    }

    // 第3步：更新状态
    m_status = 3;

    return 0;
}

int CTcpSocket::Init(const CSockParam& param) {
    // 第1步：状态检查（不变）
    if (m_status != 0) return -1;

    // 第2步：保存参数（不变）
    m_param = param;

    // 第3步：选择类型（不变）
    int type = SOCK_STREAM;

    // ========== 修改1：地址族改为AF_INET ==========
    m_socket = socket(AF_INET, type, 0);  // ← AF_UNIX → AF_INET
    if (m_socket == -1) return -2;

    // 第5步：服务器绑定+监听
    if (param.attr & SOCK_ISSERVER) {
        // ========== 修改2：不需要unlink ==========
        // unlink(param.ip);  ← 删除这行！TCP不需要删除文件

        // ========== 修改3：使用addrin()和sockaddr_in ==========
        if (bind(m_socket, param.addrin(), sizeof(sockaddr_in)) == -1) {
            //                 ↑↑↑↑↑↑↑↑↑↑  ↑↑↑↑↑↑↑↑↑↑↑↑↑
            //                 addrun→addrin   sockaddr_un→sockaddr_in
            return -3;
        }

        if (listen(m_socket, 10) == -1) {
            return -4;
        }
    }

    // 第6步：设置非阻塞（不变）
    if (param.attr & SOCK_ISBLOCK) {
        int flags = fcntl(m_socket, F_GETFL, 0);
        fcntl(m_socket, F_SETFL, flags | O_NONBLOCK);
    }

    // 第7步：更新状态（不变）
    m_status = 1;
    return 0;
}


int CTcpSocket::Link(CSocketBase * *pClient) {
    // 第1步：状态检查（不变）
    if (m_status != 1) return -1;

    // 第2步：判断服务器/客户端
    if (m_param.attr & SOCK_ISSERVER) {
        // ========== 服务器逻辑 ==========
        if (pClient == NULL) return -2;

        // ========== 修改：使用sockaddr_in ==========
        sockaddr_in client_addr;  // ← sockaddr_un → sockaddr_in
        socklen_t len = sizeof(client_addr);
        int client_fd = accept(m_socket, (sockaddr*)&client_addr, &len);

        if (client_fd == -1) return -3;

        // ========== 修改：创建CTcpSocket对象 ==========
        CTcpSocket* new_client = new CTcpSocket(client_fd);
        //          ↑↑↑↑↑↑↑↑↑
        // CLocalSocket → CTcpSocket
        new_client->m_status = 2;

        *pClient = new_client;
        return 0;

    }
    else {
        // ========== 客户端逻辑 ==========
        // ========== 修改：使用addrin()和sockaddr_in ==========
        if (connect(m_socket, m_param.addrin(), sizeof(sockaddr_in)) == -1) {
            //                    ↑↑↑↑↑↑↑↑↑↑  ↑↑↑↑↑↑↑↑↑↑↑↑↑
            //                    addrun→addrin   sockaddr_un→sockaddr_in
            return -4;
        }

        m_status = 2;
        return 0;
    }
}


//Send()、Recv()、Close() 完全不变！

// ========== Send函数（和CLocalSocket完全一样） ==========
int CTcpSocket::Send(const Buffer & data) {
    if (m_status != 2) return -1;
    return send(m_socket, (const char*)data.c_str(), data.size(), 0);
}

// ========== Recv函数（和CLocalSocket完全一样） ==========
int CTcpSocket::Recv(Buffer& data) {
    if (m_status != 2) return -1;
    return recv(m_socket, (char*)data.c_str(), data.size(), 0);
}

// ========== Close函数（和CLocalSocket完全一样） ==========
int CTcpSocket::Close() {
    if (m_status == 0 || m_status == 3) return -1;

    if (m_socket != -1) {
        int fd = m_socket;
        m_socket = -1;
        close(fd);
    }

    m_status = 3;
    return 0;
}
//Init() 函数：改为SOCK_DGRAM

int CUdpSocket::Init(const CSockParam& param) {
    if (m_status != 0) return -1;

    m_param = param;

    // ========== 修改：使用SOCK_DGRAM ==========
    int type = SOCK_DGRAM;  // ← UDP使用DGRAM（数据报）

    m_socket = socket(AF_INET, type, 0);
    if (m_socket == -1) return -2;

    // ========== UDP服务器也需要bind ==========
    if (param.attr & SOCK_ISSERVER) {
        if (bind(m_socket, param.addrin(), sizeof(sockaddr_in)) == -1) {
            return -3;
        }
        // ❌ UDP不需要listen！
        // listen(m_socket, 10);  ← 删除这行
    }

    // 设置非阻塞
    if (param.attr & SOCK_ISBLOCK) {
        int flags = fcntl(m_socket, F_GETFL, 0);
        fcntl(m_socket, F_SETFL, flags | O_NONBLOCK);
    }

    m_status = 1;
    return 0;
}


//Link() 函数：UDP不需要连接

int CUdpSocket::Link(CSocketBase * *pClient) {
    // UDP是无连接的，不需要Link
    // 直接返回成功，更新状态即可
    if (m_status != 1) return -1;

    m_status = 2;  // 设为"已连接"（实际没连接，只是标记可用）
    return 0;
}

//Send() 函数：使用sendto()

int CUdpSocket::Send(const Buffer & data) {
    if (m_status != 2) return -1;

    // ========== 使用sendto()，需要指定目标地址 ==========
    int ret = sendto(m_socket, (const char*)data.c_str(), data.size(), 0,
        m_param.addrin(), sizeof(sockaddr_in));
    //        ↑↑↑↑↑↑                           ↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑
    //        sendto（不是send）                 目标地址

    return ret;
}

//sendto() 函数：
//ssize_t sendto(int sockfd, const void* buf, size_t len, int flags,
//    const struct sockaddr* dest_addr, socklen_t addrlen);
////             ↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑
////             比send多了目标地址参数


//Recv() 函数：使用recvfrom()

int CUdpSocket::Recv(Buffer & data) {
    if (m_status != 2) return -1;

    // ========== 使用recvfrom()，可以获取发送方地址 ==========
    sockaddr_in from_addr;
    socklen_t len = sizeof(from_addr);

    int ret = recvfrom(m_socket, (char*)data.c_str(), data.size(), 0,
        (sockaddr*)&from_addr, &len);
    //        ↑↑↑↑↑↑↑↑                    ↑↑↑↑↑↑↑↑↑↑↑↑
    //        recvfrom（不是recv）          接收发送方地址

    // 可以保存发送方地址，用于回复
    // m_param.addr_in = from_addr;  // 更新目标地址

    return ret;
}

//recvfrom() 函数：
//ssize_t recvfrom(int sockfd, void* buf, size_t len, int flags,
//    struct sockaddr* src_addr, socklen_t* addrlen);
////               ↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑
////               比recv多了发送方地址参数（输出参数）


//Close() 函数：和TCP相同

int CUdpSocket::Close() {
    if (m_status == 0 || m_status == 3) return -1;

    if (m_socket != -1) {
        int fd = m_socket;
        m_socket = -1;
        close(fd);
    }

    m_status = 3;
    return 0;
}