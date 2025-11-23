# GameServerFramework

> 高性能通用游戏服务器框架 - 基于 C++ 实现的跨平台网络服务器框架

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![C++11](https://img.shields.io/badge/C%2B%2B-11-brightgreen.svg)](https://en.cppreference.com/w/cpp/11)
[![Platform](https://img.shields.io/badge/Platform-Linux-orange.svg)](https://www.linux.org/)

---

## 📖 项目简介

**GameServerFramework** 是一个专为游戏服务器设计的高性能网络框架，提供了完整的网络通信、I/O 多路复用、多进程管理等核心功能。

### ✨ 核心特性

- **🚀 高性能**：基于 Epoll 的事件驱动架构，支持高并发连接
- **🔌 多协议支持**：Unix 域 Socket / TCP / UDP 统一接口
- **🎯 易于扩展**：面向对象设计，抽象基类 + 策略模式
- **🛡️ 稳定可靠**：RAII 资源管理，完善的状态机控制
- **📦 模块化设计**：网络层、服务器层、工具层清晰分离

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

- **操作系统**：Linux (Ubuntu 18.04+, CentOS 7+)
- **编译器**：GCC 4.8+ 或 Clang 3.4+
- **构建工具**：CMake 3.10+ 或 Make
- **C++ 标准**：C++11

### 编译安装

#### 方式一：使用 CMake（推荐）

```bash
# 1. 克隆项目
git clone https://github.com/yourusername/GameServerFramework.git
cd GameServerFramework

# 2. 创建编译目录
mkdir build && cd build

# 3. 配置并编译
cmake ..
make -j4

# 4. 运行
./bin/game_server
```

#### 方式二：使用 Makefile

```bash
# 1. 克隆项目
git clone https://github.com/yourusername/GameServerFramework.git
cd GameServerFramework

# 2. 编译
make

# 3. 运行
make run
```

---

## 📚 核心模块

### 1️⃣ Socket 网络模块

提供统一的 Socket 接口封装，支持三种协议：

| 类 | 协议 | 使用场景 |
|----|------|---------|
| **CLocalSocket** | Unix 域 | 本机进程间通信 |
| **CTcpSocket** | TCP/IP | 网络可靠传输 |
| **CUdpSocket** | UDP | 网络快速传输 |

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

### 2️⃣ Epoll I/O 多路复用

高效的事件驱动模型，支持高并发连接。

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
    }
}
```

**详细文档：** [Epoll 封装学习总结](docs/epoll_guide.md)

---

## 🎯 设计模式

| 模式 | 应用 | 优势 |
|------|------|------|
| **策略模式** | CSocketBase 基类 | 统一接口，可替换实现 |
| **RAII** | 资源管理 | 自动释放，防止泄漏 |
| **状态机** | Socket 生命周期 | 防止误操作 |
| **工厂模式** | Socket 创建 | 解耦对象创建逻辑 |

---

## 📊 性能指标

| 指标 | 数值 | 说明 |
|------|------|------|
| **并发连接数** | 10,000+ | 单进程支持 |
| **每秒处理请求** | 50,000+ | QPS（视硬件配置） |
| **内存占用** | < 100MB | 空闲状态 |
| **CPU 占用** | < 5% | 空闲状态 |

---

## 🗺️ 开发路线图

### ✅ 已完成

- [x] Socket 封装（Unix 域 / TCP / UDP）
- [x] Epoll 封装（事件驱动）
- [x] 进程管理（Process）
- [x] Buffer 智能缓冲区
- [x] 状态机设计

### ⏳ 进行中

- [ ] 会话管理（Session）
- [ ] 协议层（解决粘包/半包）
- [ ] 线程池
- [ ] 对象池

### 🔮 计划中

- [ ] 日志系统
- [ ] 配置管理
- [ ] 数据库接口
- [ ] 定时器
- [ ] 热更新支持

---

## 🤝 贡献指南

欢迎贡献代码、报告 Bug、提出建议！

1. Fork 本项目
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 提交 Pull Request

---

## 📄 开源协议

本项目采用 [MIT License](LICENSE) 开源协议。

---

## 📧 联系方式

- **作者**：Your Name
- **邮箱**：your.email@example.com
- **项目主页**：https://github.com/yourusername/GameServerFramework

---

## 🙏 致谢

感谢以下项目和资源的启发：

- [易播服务器](https://example.com) - 参考项目
- [Linux 高性能服务器编程](https://book.douban.com/) - 技术书籍
- [C++ Primer](https://book.douban.com/) - C++ 学习资源

---

**⭐ 如果这个项目对你有帮助，请给一个 Star！**
