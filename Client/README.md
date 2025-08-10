# QKChat 客户端

## 项目简介

QKChat客户端是一个基于Qt6和QML开发的即时通讯应用客户端，目前主要实现了完整的用户认证系统。采用C++后端和QML前端的架构设计，支持跨平台运行。

**当前版本状态**：用户认证系统已完成，聊天功能正在开发中。

## 已实现功能

### 🔐 用户认证系统
- **用户注册**：支持用户名、邮箱注册，包含完整的表单验证
- **邮箱验证**：注册时发送验证码到邮箱进行验证
- **用户登录**：支持用户名或邮箱登录
- **密码安全**：使用盐值哈希保护用户密码
- **会话管理**：支持自动登录和记住密码功能
- **网络通信**：与服务器进行安全的TCP通信

### 🎨 用户界面
- **现代化设计**：基于QML的简洁界面设计
- **主题系统**：支持浅色/深色主题切换
- **自定义组件**：包含按钮、文本框、对话框等自定义UI组件
- **页面切换**：流畅的登录/注册页面切换动画
- **响应式提示**：友好的加载提示和错误消息显示

### 📱 用户体验
- **启动优化**：优化的应用启动流程，避免界面卡顿
- **自动连接**：启动时自动尝试连接服务器
- **设置持久化**：用户设置和登录状态本地存储
- **错误处理**：完善的网络错误和异常处理机制

## 技术架构

### 核心技术栈
- **Qt6**：跨平台应用框架
- **QML**：声明式用户界面语言
- **C++17**：高性能后端逻辑
- **SQLite**：本地数据存储
- **OpenSSL**：加密通信支持

### 项目结构
```
Client/
├── main.cpp                 # 应用程序入口
├── Main.qml                # 主窗口QML文件
├── CMakeLists.txt          # CMake构建配置
├── qml/                    # QML界面文件
│   ├── LoginPage.qml       # 登录页面
│   ├── RegisterPage.qml    # 注册页面
│   ├── pages/              # 页面组件
│   │   └── MainPage.qml    # 主页面
│   └── components/         # 通用组件
│       ├── ThemeManager.qml    # 主题管理
│       ├── LoadingDialog.qml   # 加载对话框
│       └── MessageDialog.qml   # 消息对话框
└── src/                    # C++源代码
    ├── auth/               # 认证模块
    │   ├── AuthManager.h/cpp      # 认证管理器
    │   ├── NetworkClient.h/cpp    # 网络客户端
    │   └── SessionManager.h/cpp   # 会话管理
    ├── models/             # 数据模型
    │   ├── User.h/cpp             # 用户模型
    │   └── AuthResponse.h/cpp     # 认证响应模型
    ├── utils/              # 工具类
    │   ├── Logger.h/cpp           # 日志系统
    │   ├── Crypto.h/cpp           # 加密工具
    │   └── Validator.h/cpp        # 数据验证
    └── DatabaseManager.h/cpp      # 数据库管理
```

### 核心模块

#### 认证模块 (auth/)
- **AuthManager**：认证流程管理，协调登录、注册等操作
- **NetworkClient**：网络通信客户端，处理与服务器的TCP连接
- **SessionManager**：会话状态管理，处理自动登录和会话过期

#### 数据模型 (models/)
- **User**：用户信息模型，包含用户基本信息和状态
- **AuthResponse**：认证响应模型，处理服务器返回的认证结果

#### 工具模块 (utils/)
- **Logger**：日志记录系统，支持不同级别的日志输出
- **Crypto**：加密工具类，提供密码哈希和验证功能
- **Validator**：数据验证工具，验证用户输入的有效性

#### 数据库管理
- **DatabaseManager**：SQLite数据库管理，处理本地数据存储
- 支持用户信息、聊天记录、应用设置等数据管理

## 配置说明

### 服务器连接配置
客户端默认连接到本地服务器（localhost:8080），可在`main.cpp`中修改：

```cpp
authManager->initialize("localhost", 8080, true);
```

### 主题配置
主题设置保存在本地，支持：
- 浅色主题：适合明亮环境
- 深色主题：适合夜间使用
- 自动切换：根据系统设置自动切换

### 日志配置
日志文件保存在应用数据目录：
- Windows: `%APPDATA%/QKChat/Client/logs/`
- macOS: `~/Library/Application Support/QKChat/Client/logs/`
- Linux: `~/.local/share/QKChat/Client/logs/`