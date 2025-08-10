# QKChat Server

QKChat是一个基于Qt6和C++开发的通讯服务器，目前主要实现了完整的用户认证系统和基础网络通信框架。

## 项目概述

QKChat Server是一个正在开发中的通讯服务器，采用现代化的架构设计，当前版本已完成用户认证系统、邮件服务、网络通信基础设施等核心功能。服务器使用Qt6框架开发，具备良好的跨平台性能和稳定性。

**当前版本状态**：用户认证系统已完成，聊天功能正在开发中。

## 已实现功能

### 用户认证系统
- **用户注册与登录**：支持用户名/邮箱登录，完整的表单验证
- **密码安全**：使用盐值哈希算法保护用户密码
- **邮箱验证**：注册时发送验证码邮件，支持多种验证场景
- **会话管理**：基于令牌的安全会话机制
- **心跳检测**：客户端连接状态监控

### 网络通信
- **TCP服务器**：基于Qt的高性能TCP服务器
- **客户端管理**：多客户端连接管理和消息路由
- **协议处理**：统一的JSON消息协议处理
- **SSL/TLS支持**：可选的加密通信支持
- **连接监控**：实时连接状态和性能监控

### 数据管理
- **MySQL数据库**：用户数据、系统配置存储
- **Redis缓存**：会话管理、验证码缓存
- **数据库连接池**：高效的数据库连接管理
- **数据验证**：完善的输入数据验证和安全检查

### 邮件服务
- **SMTP客户端**：自定义SMTP客户端实现
- **验证邮件**：用户注册、密码重置验证码发送
- **邮件模板**：美观的HTML邮件模板
- **多邮件服务商支持**：支持各种SMTP服务配置

### 系统管理
- **配置管理**：灵活的JSON配置文件系统
- **日志系统**：分级日志记录，支持文件和控制台输出
- **性能监控**：请求处理时间和系统性能统计
- **错误处理**：完善的异常处理和错误恢复机制
- **安全审计**：用户操作和安全事件记录

## 系统架构

### 核心组件

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