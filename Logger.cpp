#include "Logger.h"
// Start方法（占位，下次详细实现）
 int CLoggerServer::Start() {
    // 第1步：检查是否已启动
    if (m_server != NULL) return -1;

    // 第2步：创建日志目录
    if (access("log", W_OK | R_OK) != 0) {
        mkdir("log", S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
    }
    //// mode参数（权限位）：
    //S_IRUSR   // User Read    (400)  所有者可读
    //    S_IWUSR   // User Write   (200)  所有者可写
    //    S_IRGRP   // Group Read   (040)  组可读
    //    S_IWGRP   // Group Write  (020)  组可写
    //    S_IROTH   // Other Read   (004)  其他人可读

    // 第3步：打开日志文件
    m_file = fopen(m_path, "w+");
    if (m_file == NULL) return -2;
    //// mode参数：
    //"r"   // 只读，文件必须存在
    //    "w"   // 只写，文件不存在则创建，存在则清空
    //    "a"   // 追加，文件不存在则创建
    //    "w+"  // ⭐ 读写，文件不存在则创建，存在则清空
    //    "a+"  // 读写，追加模式

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

    // 第7步：启动日志线程
    ret = m_thread.Start();
    if (ret != 0) {
        Close();
        return -6;
    }

    return 0;
}