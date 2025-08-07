# QKChat Server

QKChat是一个基于Qt6和C++开发的通讯服务器，提供聊天服务。

## 📋 项目概述

QKChat Server是一个功能完整的通讯服务器，采用现代化的架构设计，支持多客户端连接、实时消息传递、用户认证、文件传输等功能。服务器使用Qt6框架开发，具备良好的跨平台性能和稳定性。

## ✨ 主要特性

### 🔐 安全认证
- **用户注册与登录**：支持用户名/邮箱登录
- **密码加密**：使用盐值哈希算法保护用户密码
- **邮箱验证**：注册时发送验证邮件
- **会话管理**：安全的会话令牌机制
- **TLS加密**：支持SSL/TLS加密通信

### 💬 即时通讯
- **实时消息**：基于TCP的实时消息传递
- **群组聊天**：支持群组消息和群组管理
- **私聊功能**：用户间私密对话
- **消息历史**：消息持久化存储
- **在线状态**：实时显示用户在线状态

### 🗄️ 数据管理
- **MySQL数据库**：用户数据、消息历史存储
- **Redis缓存**：会话管理、在线状态缓存
- **连接池**：高效的数据库连接管理
- **数据备份**：自动数据备份和恢复

### 📧 邮件服务
- **SMTP集成**：支持多种邮件服务商
- **验证邮件**：用户注册验证
- **通知邮件**：重要事件通知
- **邮件模板**：可定制的邮件模板

### 🔧 系统管理
- **服务器监控**：实时性能监控
- **日志系统**：分级日志记录
- **配置管理**：灵活的配置文件
- **错误处理**：完善的错误处理机制

## 🏗️ 系统架构

### 核心组件

```
QKChat Server
├── ServerManager          # 服务器管理器（核心协调器）
├── Network Layer         # 网络通信层
│   ├── TcpServer        # TCP服务器
│   ├── ClientHandler    # 客户端处理器
│   └── ProtocolHandler  # 协议处理器
├── Authentication       # 认证模块
│   ├── UserService      # 用户服务
│   ├── EmailService     # 邮件服务
│   └── SmtpClient       # SMTP客户端
├── Database Layer       # 数据层
│   ├── DatabaseManager  # 数据库管理器
│   └── RedisClient      # Redis客户端
├── Security             # 安全模块
│   ├── CertificateManager # 证书管理
│   ├── OpenSSLHelper    # OpenSSL助手
│   └── Crypto           # 加密工具
└── Utils               # 工具模块
    ├── Logger           # 日志系统
    ├── Validator        # 数据验证
    └── ConfigManager    # 配置管理
```

### 技术栈

- **开发框架**：Qt6 (C++)
- **数据库**：MySQL 8.0+
- **缓存**：Redis 6.0+
- **加密**：OpenSSL 3.0+
- **构建系统**：CMake 3.16+
- **编译器**：MinGW

## ⚙️ 配置说明

### 服务器配置

主要配置文件位于 `config/server.json`：

### 环境变量

| 变量名 | 描述 | 默认值 |
|--------|------|--------|
| `QKCHAT_CONFIG_PATH` | 配置文件路径 | `./config/server.json` |
| `QKCHAT_LOG_LEVEL` | 日志级别 | `INFO` |
| `QKCHAT_DB_HOST` | 数据库主机 | `localhost` |
| `QKCHAT_REDIS_HOST` | Redis主机 | `localhost` |


### 服务器管理

- **查看状态**：通过GUI界面查看服务器状态
- **监控连接**：实时查看客户端连接数
- **日志查看**：查看系统日志和错误信息

### 客户端连接

客户端可以通过以下方式连接：
- **TCP连接**：`tcp://localhost:8080`
- **TLS连接**：`tls://localhost:8080`（推荐）

### 日志分析

日志文件位置：
- `logs/server.log` - 主日志文件
- `logs/network.log` - 网络模块日志
- `logs/database.log` - 数据库日志
- `logs/auth.log` - 认证模块日志