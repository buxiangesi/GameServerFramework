// Logger.cpp - 日志模块实现
#include "Logger.h"

// ==================== CLoggerServer::Start ====================
int CLoggerServer::Start() {
    // 第1步：检查是否已启动
    if (m_server != NULL) return -1;

    // 第2步：创建日志目录
    if (access("log", W_OK | R_OK) != 0) {
        // ⚡ 修复：添加执行权限（X），否则无法访问目录
        // 0755 = rwxr-xr-x（用户：读写执行，组：读执行，其他：读执行）
        mkdir("log", 0755);
    }

    // 第3步：打开日志文件
    m_file = fopen(m_path, "w+");
    if (m_file == NULL) return -2;

    // 第4步：创建epoll实例
    int ret = m_epoll.Create(1);
    if (ret != 0) return -3;

    // 第5步：创建服务器Socket
    m_server = new CLocalSocket();
    if (m_server == NULL) {
        Close();
        return -4;
    }

    // 第6步：初始化Socket（绑定路径）
    ret = m_server->Init(CSockParam("./log/server.sock", (int)SOCK_ISSERVER));
    if (ret != 0) {
        Close();
        return -5;
    }

    // ⚡ 第7步：把服务器socket添加到epoll监听（关键！）
    ret = m_epoll.Add(*m_server, EpollData((void*)m_server), EPOLLIN | EPOLLERR);
    if (ret != 0) {
        Close();
        return -6;
    }

    // 第8步：启动日志线程
    ret = m_thread.Start();
    if (ret != 0) {
        Close();
        return -7;
    }

    return 0;
}

// ==================== CLoggerServer::WriteLog ====================
void CLoggerServer::WriteLog(const Buffer& data) {
    if (m_file != NULL) {
        fwrite((char*)data, 1, data.size(), m_file);
        fflush(m_file);
#ifdef _DEBUG
        printf("%s", (char*)data);
#endif
    }
}

// ==================== LogInfo构造函数1：printf风格 ====================
LogInfo::LogInfo(
    const char* file, int line, const char* func,
    pid_t pid, pthread_t tid, int level,
    const char* fmt, ...
)
{
    const char sLevel[][8] = {
        "INFO","DEBUG","WARNING","ERROR","FATAL"
    };

    char* buf = NULL;
    bAuto = false;  // printf风格需要手动调用Trace()

    // 格式化日志头
    int count = asprintf(&buf, "%s(%d):[%s][%s]<%d-%d>(%s) ",
        file, line, sLevel[level],
        (char*)CLoggerServer::GetTimeStr(), pid, tid, func);

    if (count > 0) {
        m_buf = buf;
        free(buf);
    }
    else return;

    // 格式化用户消息
    va_list ap;
    va_start(ap, fmt);
    count = vasprintf(&buf, fmt, ap);
    if (count > 0) {
        m_buf += buf;
        free(buf);
    }
    va_end(ap);

    // ⚡ 添加换行符（重要！）
    m_buf += "\n";
}

// ==================== LogInfo构造函数2：流式输出风格 ====================
LogInfo::LogInfo(
    const char* file, int line, const char* func,
    pid_t pid, pthread_t tid, int level
)
{
    bAuto = true;  // 流式输出需要析构时调用Trace()

    const char sLevel[][8] = {
        "INFO","DEBUG","WARNING","ERROR","FATAL"
    };

    char* buf = NULL;
    int count = asprintf(&buf, "%s(%d):[%s][%s]<%d-%d>(%s) ",
        file, line, sLevel[level],
        (char*)CLoggerServer::GetTimeStr(), pid, tid, func);

    if (count > 0) {
        m_buf = buf;
        free(buf);
    }
}

// ==================== LogInfo构造函数3：dump风格 ====================
LogInfo::LogInfo(
    const char* file, int line, const char* func,
    pid_t pid, pthread_t tid, int level,
    void* pData, size_t nSize
)
{
    bAuto = false;  // dump风格需要手动调用Trace()

    const char sLevel[][8] = {
        "INFO","DEBUG","WARNING","ERROR","FATAL"
    };

    char* buf = NULL;
    int count = asprintf(&buf, "%s(%d):[%s][%s]<%d-%d>(%s)\n",
        file, line, sLevel[level],
        (char*)CLoggerServer::GetTimeStr(), pid, tid, func);

    if (count > 0) {
        m_buf = buf;
        free(buf);
    }
    else return;

    // 转换二进制数据为十六进制
    char* Data = (char*)pData;
    for (size_t i = 0; i < nSize; i++) {
        char buf[16] = "";
        snprintf(buf, sizeof(buf), "%02X ", Data[i] & 0xFF);
        m_buf += buf;

        if (0 == ((i + 1) % 16)) {
            m_buf += "\t; ";
            for (size_t j = i - 15; j <= i; j++) {
                if ((Data[j] & 0xFF) > 31 && (Data[j] & 0xFF) < 0x7F) {
                    m_buf += Data[j];
                }
                else {
                    m_buf += '.';
                }
            }
            m_buf += "\n";
        }
    }

    // 处理末尾不足16字节
    size_t k = nSize % 16;
    if (k != 0) {
        for (size_t j = 0; j < 16 - k; j++)
            m_buf += "   ";
        m_buf += "\t; ";
        for (size_t j = nSize - k; j < nSize; j++) {
            if ((Data[j] & 0xFF) > 31 && (Data[j] & 0xFF) < 0x7F) {
                m_buf += Data[j];
            }
            else {
                m_buf += '.';
            }
        }
        m_buf += "\n";
    }
}

// ==================== LogInfo析构函数 ====================
LogInfo::~LogInfo()
{
    if (bAuto) {
        // ⚡ 流式输出：添加换行符（重要！）
        m_buf += "\n";
        CLoggerServer::Trace(*this);
    }
}

// ==================== CLoggerServer::Trace实现 ====================
void CLoggerServer::Trace(const LogInfo& info) {
    static thread_local CLocalSocket client;

    if (client == -1) {
        // 第1步：初始化socket
        int ret = client.Init(CSockParam("./log/server.sock", 0));
        if (ret != 0) {
#ifdef _DEBUG
            printf("%s(%d):[%s]初始化socket失败 ret=%d\n",
                __FILE__, __LINE__, __FUNCTION__, ret);
#endif
            return;
        }
#ifdef _DEBUG
        printf("%s(%d):[%s]初始化成功 client=%d\n",
            __FILE__, __LINE__, __FUNCTION__, (int)client);
#endif

        // ⚡ 第2步：连接到日志服务器（关键！）
        ret = client.Link();
        if (ret != 0) {
#ifdef _DEBUG
            printf("%s(%d):[%s]连接日志服务器失败 ret=%d\n",
                __FILE__, __LINE__, __FUNCTION__, ret);
#endif
            return;
        }
#ifdef _DEBUG
        printf("%s(%d):[%s]连接成功 client=%d\n",
            __FILE__, __LINE__, __FUNCTION__, (int)client);
#endif
    }

    // 第3步：发送日志数据
    int ret = client.Send(info);
#ifdef _DEBUG
    printf("%s(%d):[%s]发送日志 ret=%d size=%zu\n",
        __FILE__, __LINE__, __FUNCTION__, ret, ((Buffer)info).size());
#endif
}
