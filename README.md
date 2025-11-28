# GameServerFramework

> 高性能通用游戏服务器框架 - 基于 C++ 实现的跨平台网络服务器框架

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![C++11](https://img.shields.io/badge/C%2B%2B-11-brightgreen.svg)](https://en.cppreference.com/w/cpp/11)
[![Platform](https://img.shields.io/badge/Platform-Linux-orange.svg)](https://www.linux.org/)
[![Progress](https://img.shields.io/badge/Progress-30%25-yellow.svg)](https://github.com/buxiangesi/GameServerFramework)
[![Status](https://img.shields.io/badge/Status-In%20Development-blue.svg)](https://github.com/buxiangesi/GameServerFramework)

---

## 📊 项目状态

**当前版本**：v0.3.0-alpha
**开发阶段**：基础网络层（Phase 1）
**整体进度**：30% ████░░░░░░

| 模块 | 状态 | 进度 | 说明 |
|------|------|------|------|
| **网络层** | ✅ 完成 | 100% | Socket封装 + Epoll封装 |
| **协议层** | 🚧 规划中 | 0% | 待实现粘包/半包处理 |
| **会话层** | 📝 设计中 | 0% | Session管理设计完成 |
| **服务器核心** | 📝 设计中 | 0% | GameServer框架设计完成 |
| **线程池** | ⏳ 待开发 | 0% | 架构文档已完成 |
| **日志系统** | ⏳ 待开发 | 0% | - |
| **配置管理** | ⏳ 待开发 | 0% | 配置模板已创建 |

**最近更新**：2025-11-24 - 完成项目重构，建立专业框架结构

---

## 📖 项目简介

**GameServerFramework** 是一个正在开发中的高性能游戏服务器框架，目标是提供完整的网络通信、会话管理、业务逻辑处理等核心功能。

### ✨ 已实现特性

- ✅ **Socket 统一封装**：Unix域 / TCP / UDP 三种协议统一接口
- ✅ **Epoll 事件驱动**：高效的 I/O 多路复用，支持高并发
- ✅ **状态机管理**：完善的 Socket 生命周期控制
- ✅ **RAII 资源管理**：自动内存管理，防止资源泄漏
- ✅ **模块化设计**：网络层独立封装，易于扩展

### 🚧 正在开发

- 🚧 **协议层设计**：解决 TCP 粘包/半包问题（规划阶段）
- 🚧 **会话管理**：客户端连接管理（设计阶段）
- 🚧 **服务器核心**：GameServer 主类实现（设计阶段）

### 🔮 计划功能

- 📝 线程池 - 多线程业务处理
- 📝 对象池 / 内存池 - 性能优化
- 📝 日志系统 - 运行状态记录
- 📝 配置管理 - 动态配置加载
- 📝 定时器 - 定时任务调度
- 📝 数据库接口 - MySQL/Redis 支持

---

## 🏗️ 项目结构

```
GameServerFramework/
├── README.md                    # 项目说明文档
├── CMakeLists.txt               # CMake 构建配置
├── Makefile                     # Make 构建配置
├── .gitignore                   # Git 忽略文件
│
├── docs/                        # 📚 文档目录
│   ├── socket_guide.md          # Socket 封装学习总结
│   ├── epoll_guide.md           # Epoll 封装学习总结
│   └── git_guide.md             # Git 版本管理指南
│
├── include/                     # 📦 头文件目录
│   ├── network/                 # 网络模块
│   │   ├── Socket.h             # Socket 基类和实现
│   │   └── Epoll.h              # Epoll 封装
│   ├── common/                  # 通用工具
│   └── server/                  # 服务器核心
│
├── src/                         # 💻 源代码目录
│   ├── network/                 # 网络模块实现
│   ├── common/                  # 通用工具实现
│   ├── server/                  # 服务器核心实现
│   └── main.cpp                 # 主程序入口
│
├── examples/                    # 📖 示例代码
├── tests/                       # 🧪 测试代码
├── build/                       # 🔨 编译输出目录
└── config/                      # ⚙️ 配置文件目录
```

---

## 🚀 快速开始

### 环境要求

- **操作系统**：Linux (Ubuntu 18.04+, CentOS 7+) / Windows + WSL
- **编译器**：GCC 4.8+ 或 Clang 3.4+ 或 MSVC 2019+
- **构建工具**：CMake 3.10+ 或 Make
- **C++ 标准**：C++11
- **IDE**：Visual Studio 2019/2022（Windows）、VSCode、CLion

### 编译安装

#### 方式一：Visual Studio（Windows 推荐）

```bash
1. 打开 Visual Studio 2019/2022
2. 选择 "打开本地文件夹"
3. 选择 GameServerFramework 目录
4. VS 自动识别 CMakeLists.txt 并配置
5. 按 Ctrl+Shift+B 编译
6. 按 F5 运行
```

#### 方式二：使用 CMake（Linux 推荐）

```bash
# 1. 克隆项目
git clone https://github.com/buxiangesi/GameServerFramework.git
cd GameServerFramework

# 2. 创建编译目录
mkdir build && cd build

# 3. 配置并编译
cmake ..
make -j4

# 4. 运行
./bin/game_server
```

#### 方式三：使用 Makefile

```bash
# 1. 克隆项目
git clone https://github.com/buxiangesi/GameServerFramework.git
cd GameServerFramework

# 2. 编译
make

# 3. 运行
make run
```

**注意**：当前版本处于早期开发阶段，主程序为测试代码，用于验证网络层功能。

---

## 📚 核心模块

### ✅ 1️⃣ Socket 网络模块（已完成 - 100%）

提供统一的 Socket 接口封装，支持三种协议：

| 类 | 协议 | 使用场景 | 状态 |
|----|------|---------|------|
| **CLocalSocket** | Unix 域 Socket | 本机进程间通信 | ✅ 已实现 |
| **CTcpSocket** | TCP/IP | 网络可靠传输 | ✅ 已实现 |
| **CUdpSocket** | UDP | 网络快速传输 | ✅ 已实现 |

**已实现功能：**
- ✅ Init() - 初始化 Socket（创建、绑定、监听、非阻塞设置）
- ✅ Link() - 服务端 accept / 客户端 connect
- ✅ Send() - 发送数据
- ✅ Recv() - 接收数据
- ✅ Close() - 关闭连接
- ✅ 状态机管理（未初始化→已初始化→已连接→已关闭）
- ✅ RAII 自动资源管理

**示例代码：**

```cpp
#include "network/Socket.h"

// 创建 TCP 服务器
CTcpSocket server;
server.Init(CSockParam("0.0.0.0", 8080, SOCK_ISSERVER));

// 等待客户端连接
CSocketBase* client = nullptr;
server.Link(&client);

// 通信
Buffer data(1024);
client->Recv(data);
client->Send("Hello Client!");
```

**详细文档：** [Socket 封装学习总结](docs/socket_guide.md)

---

### ✅ 2️⃣ Epoll I/O 多路复用（已完成 - 100%）

高效的事件驱动模型，支持高并发连接。

**已实现功能：**
- ✅ Create() - 创建 Epoll 实例
- ✅ Add() - 添加文件描述符到 Epoll
- ✅ Modify() - 修改监听事件
- ✅ Del() - 删除文件描述符
- ✅ WaitEvents() - 等待事件触发
- ✅ EpollData 数据封装（支持 fd 和 void* 指针）

**示例代码：**

```cpp
#include "network/Epoll.h"

// 创建 Epoll 实例
CEpoll epoll;
epoll.Create(1024);

// 添加监听
epoll.Add(server_fd, EpollData(server_fd), EPOLLIN);

// 事件循环
EPEvents events;
while (true) {
    int n = epoll.WaitEvents(events);
    for (int i = 0; i < n; i++) {
        // 处理事件
        if (events[i].events & EPOLLIN) {
            // 处理读事件
        }
    }
}
```

**详细文档：** [Epoll 封装学习总结](docs/epoll_guide.md)

---

### 🚧 3️⃣ 协议层（规划中 - 0%）

**目标**：解决 TCP 粘包/半包问题

**设计方案：**
- 长度前缀协议：[4字节长度] + [数据内容]
- 自动粘包/半包处理
- 消息队列管理

**待实现：**
- [ ] CProtocol 类设计
- [ ] 粘包问题测试用例
- [ ] 半包问题测试用例

---

### 📝 4️⃣ 会话管理（设计中 - 0%）

**目标**：管理客户端连接会话

**设计方案：**（参考 [架构设计文档](docs/architecture.md)）
```cpp
class Session {
    int m_fd;                  // 文件描述符
    uint64_t m_session_id;     // 会话 ID
    Buffer m_recv_buffer;      // 接收缓冲区
    Buffer m_send_buffer;      // 发送缓冲区
};
```

**待实现：**
- [ ] Session 类实现
- [ ] 会话超时管理
- [ ] 缓冲区管理

---

### 📝 5️⃣ GameServer 核心（设计中 - 0%）

**目标**：游戏服务器主类

**设计方案：**（参考 [架构设计文档](docs/architecture.md)）
```cpp
class GameServer {
    CEpoll m_epoll;                      // Epoll 实例
    CSocketBase* m_listen_socket;        // 监听 Socket
    std::map<int, Session*> m_sessions;  // 会话管理
};
```

**待实现：**
- [ ] GameServer 类实现
- [ ] 事件循环主逻辑
- [ ] OnAccept / OnRecv / OnDisconnect 回调
- [ ] 配置文件加载

---

## 🎯 设计模式

| 模式 | 应用位置 | 状态 | 优势说明 |
|------|---------|------|---------|
| **策略模式** | CSocketBase 抽象基类 | ✅ 已应用 | 统一接口，三种 Socket 可替换实现 |
| **RAII** | Socket/Epoll 资源管理 | ✅ 已应用 | 析构函数自动释放，防止资源泄漏 |
| **状态机** | Socket 生命周期控制 | ✅ 已应用 | 4 状态转换，防止非法操作 |
| **单例模式** | GameServer 主类 | 📝 设计中 | 全局唯一服务器实例 |
| **观察者模式** | 事件回调系统 | 📝 计划中 | OnAccept/OnRecv/OnDisconnect |
| **对象池模式** | Session/Buffer 复用 | 📝 计划中 | 减少频繁 new/delete 开销 |

---

## 📊 性能指标

**注意**：以下为目标性能指标，当前版本仅完成网络层，完整性能测试待服务器核心实现后进行。

| 指标 | 目标值 | 当前状态 | 说明 |
|------|--------|---------|------|
| **并发连接数** | 10,000+ | 测试待进行 | Epoll 理论支持，需压测验证 |
| **每秒处理请求** | 50,000+ QPS | 测试待进行 | 取决于业务逻辑复杂度 |
| **内存占用** | < 100MB | 测试待进行 | 空闲状态（10,000 连接） |
| **CPU 占用** | < 10% | 测试待进行 | 空闲状态（单核） |
| **延迟** | < 1ms | 测试待进行 | 网络层处理延迟 |

**已完成的基础测试：**
- ✅ Socket 基本功能测试（Init/Link/Send/Recv/Close）
- ✅ Epoll 事件驱动测试（Add/Modify/Del/WaitEvents）
- ⏳ 高并发压力测试（待 GameServer 实现）

---

## 🗺️ 开发路线图

### Phase 1: 基础网络层 ✅ 已完成（2025-11）

- [x] **Socket 统一封装**（Unix 域 / TCP / UDP）
- [x] **Epoll I/O 多路复用** 封装
- [x] **Buffer 智能缓冲区**（继承 std::string）
- [x] **状态机设计**（Socket 生命周期管理）
- [x] **项目架构搭建**（CMake + 文档体系）

**里程碑**：✅ v0.3.0-alpha - 网络层封装完成

---

### Phase 2: 协议与会话层 🚧 规划中（预计 2025-12）

- [ ] **协议层实现**
  - [ ] 长度前缀协议设计
  - [ ] 粘包/半包问题解决
  - [ ] 消息序列化/反序列化
- [ ] **Session 会话管理**
  - [ ] Session 类实现
  - [ ] 会话超时机制
  - [ ] 接收/发送缓冲区管理
- [ ] **测试用例编写**
  - [ ] 单元测试（Socket/Epoll）
  - [ ] 协议层测试（粘包/半包场景）

**里程碑目标**：v0.5.0-alpha - 协议层与会话管理完成

---

### Phase 3: 服务器核心 📝 设计中（预计 2026-01）

- [ ] **GameServer 主类实现**
  - [ ] 事件循环主逻辑
  - [ ] OnAccept / OnRecv / OnDisconnect 回调
  - [ ] 配置文件加载与管理
- [ ] **基础示例程序**
  - [ ] Echo 服务器
  - [ ] 简单聊天室服务器
- [ ] **性能测试**
  - [ ] 并发连接数测试
  - [ ] QPS 压力测试
  - [ ] 内存/CPU 性能分析

**里程碑目标**：v0.8.0-beta - 可运行的游戏服务器框架

---

### Phase 4: 性能优化 ⏳ 计划中（预计 2026-02）

- [ ] **线程池** - 多线程业务处理
- [ ] **对象池 / 内存池** - 减少内存分配开销
- [ ] **无锁队列** - 线程间通信优化
- [ ] **协程支持**（可选）- 异步 I/O 优化

**里程碑目标**：v1.0.0-rc - 高性能版本

---

### Phase 5: 功能完善 🔮 远期计划（2026-03+）

- [ ] **日志系统** - 分级日志、日志轮转
- [ ] **定时器模块** - 时间轮 / 最小堆实现
- [ ] **数据库接口** - MySQL / Redis 连接池
- [ ] **HTTP 支持** - RESTful API 接口
- [ ] **热更新支持** - 配置热加载、逻辑热更新
- [ ] **监控系统** - 运行状态监控、性能指标采集

**里程碑目标**：v1.5.0 - 功能完整的游戏服务器框架

---

## 🤝 贡献指南

欢迎贡献代码、报告 Bug、提出建议！

### 如何贡献

1. **Fork 本项目**
2. **创建特性分支**
   ```bash
   git checkout -b feature/AmazingFeature
   ```
3. **提交更改**
   ```bash
   git commit -m 'feat: Add some AmazingFeature'
   ```
4. **推送到分支**
   ```bash
   git push origin feature/AmazingFeature
   ```
5. **提交 Pull Request**

### 提交规范

采用 Conventional Commits 规范：

- `feat:` - 新功能
- `fix:` - Bug 修复
- `docs:` - 文档更新
- `refactor:` - 重构代码
- `test:` - 测试相关
- `chore:` - 构建/工具链相关

**示例：**
```
feat: 实现 Session 会话管理模块
fix: 修复 Socket 关闭时的内存泄漏
docs: 更新 Epoll 封装学习文档
```

---

## 📄 开源协议

本项目采用 [MIT License](LICENSE) 开源协议。

```
MIT License

Copyright (c) 2025 GameServerFramework Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction...
```

---

## 📧 联系方式

- **项目主页**：https://github.com/buxiangesi/GameServerFramework
- **问题反馈**：https://github.com/buxiangesi/GameServerFramework/issues
- **作者**：东北大学本科生（985高校）
- **邮箱**：1258832751@qq.com

---

## 📚 学习资源

项目文档目录：
- [Socket 封装学习总结](docs/socket_guide.md) - 详细的 Socket 实现文档
- [Epoll 封装学习总结](docs/epoll_guide.md) - Epoll I/O 多路复用详解
- [架构设计文档](docs/architecture.md) - 系统架构与模块设计
- [Git 版本管理指南](docs/git_guide.md) - Git 使用指南

参考书籍与资源：
- 《Linux 高性能服务器编程》- 游双
- 《UNIX 网络编程（卷1）》- W. Richard Stevens
- 《C++ Primer（第5版）》- Stanley B. Lippman

---

## 🙏 致谢

感谢以下项目和资源的启发：

- **易播服务器** - 项目参考与学习来源
- **muduo** - 陈硕的 C++ 网络库，架构设计参考
- **libevent** - 高性能事件驱动库
- **nginx** - 高性能 Web 服务器，事件模型参考

---

## 📈 项目统计

**代码统计**（截至 2025-11-24）：

| 语言 | 文件数 | 代码行数 | 注释行数 |
|------|--------|---------|---------|
| C++ Header | 2 | ~800 | ~200 |
| C++ Source | 2 | ~600 | ~150 |
| Markdown | 4 | ~2000 | - |
| CMake | 1 | ~60 | ~20 |
| Makefile | 1 | ~85 | ~15 |
| **总计** | **10** | **~3545** | **~385** |

**开发历程：**
- 2025-11-20：启动项目，完成 Socket 封装
- 2025-11-22：完成 Epoll 封装
- 2025-11-23：完成项目架构搭建
- 2025-11-24：重构为 GameServerFramework 专业框架

---

## ⭐ Star History

如果这个项目对你有帮助，请给一个 Star！

[![Star History Chart](https://api.star-history.com/svg?repos=buxiangesi/GameServerFramework&type=Date)](https://star-history.com/#buxiangesi/GameServerFramework&Date)

---

<div align="center">

**🎮 GameServerFramework - 打造高性能游戏服务器 🚀**

Made with ❤️ by [buxiangesi](https://github.com/buxiangesi)

[⬆ 回到顶部](#gameserverframework)

</div>
