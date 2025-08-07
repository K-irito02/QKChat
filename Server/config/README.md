# QKChat Server 配置文档

## 配置文件位置
配置文件位于 `Server/config/server.json`

## 配置项说明

### 服务器配置 (server)
```json
{
  "server": {
    "port": 8080,                    // 服务器监听端口
    "max_clients": 1000,             // 最大客户端连接数
    "heartbeat_interval": 30000,     // 心跳检测间隔（毫秒）
    "use_tls": true,                 // 是否使用TLS加密
    "cert_file": "certs/server.crt", // TLS证书文件路径
    "key_file": "certs/server.key"   // TLS私钥文件路径
  }
}
```

### 数据库配置 (database)
```json
{
  "database": {
    "host": "localhost",             // 数据库服务器地址
    "port": 3306,                   // 数据库端口
    "name": "qkchat",               // 数据库名称
    "username": "root",              // 数据库用户名
    "password": "3143285505",        // 数据库密码
    "charset": "utf8mb4",           // 字符集
    "pool_size": 10,                // 连接池大小
    "timeout": 30000,               // 连接超时时间（毫秒）
    "auto_reconnect": true          // 是否自动重连
  }
}
```

### Redis配置 (redis)
```json
{
  "redis": {
    "host": "localhost",             // Redis服务器地址
    "port": 6379,                   // Redis端口
    "password": "",                  // Redis密码（可选）
    "database": 0,                  // Redis数据库编号
    "timeout": 5000,                // 连接超时时间（毫秒）
    "pool_size": 5                  // 连接池大小
  }
}
```

### SMTP配置 (smtp)
```json
{
  "smtp": {
    "host": "smtp.qq.com",          // SMTP服务器地址
    "port": 587,                    // SMTP端口
    "username": "saokiritoasuna00@qq.com", // 邮箱用户名
    "password": "ssvbzaqvotjcchjh", // 邮箱密码或授权码
    "use_tls": true,                // 是否使用TLS加密
    "from_name": "QKChat Server",   // 发件人名称
    "rate_limit": {
      "interval_seconds": 60,        // 发送间隔（秒）
      "max_per_hour": 10            // 每小时最大发送数
    }
  }
}
```

### 日志配置 (logging)
```json
{
  "logging": {
    "level": "INFO",                // 日志级别 (DEBUG, INFO, WARNING, ERROR)
    "console_output": true,         // 是否输出到控制台
    "json_format": false,           // 是否使用JSON格式
    "max_file_size": 104857600,     // 最大日志文件大小（字节）
    "retention_days": 30,           // 日志保留天数
    "directory": "",                // 日志目录（空表示使用默认目录）
    "modules": {
      "network": {
        "level": "DEBUG",
        "separate_file": true
      },
      "database": {
        "level": "INFO",
        "separate_file": true
      },
      "auth": {
        "level": "INFO",
        "separate_file": true
      }
    }
  }
}
```

### 安全配置 (security)
```json
{
  "security": {
    "rate_limit_enabled": true,     // 是否启用频率限制
    "max_requests_per_minute": 60,  // 每分钟最大请求数
    "session_timeout": 86400,       // 会话超时时间（秒）
    "password_min_length": 6,       // 密码最小长度
    "password_require_special": false, // 密码是否要求特殊字符
    "max_login_attempts": 5,        // 最大登录尝试次数
    "lockout_duration": 900,        // 锁定持续时间（秒）
    "ip_whitelist": [],             // IP白名单
    "ip_blacklist": [],             // IP黑名单
    "csrf_protection": true,        // 是否启用CSRF保护
    "xss_protection": true          // 是否启用XSS保护
  }
}
```

### 功能配置 (features)
```json
{
  "features": {
    "registration_enabled": true,    // 是否启用用户注册
    "email_verification_required": true, // 是否要求邮箱验证
    "guest_access": false,          // 是否允许访客访问
    "file_upload": {
      "enabled": true,              // 是否启用文件上传
      "max_size": 10485760,         // 最大文件大小（字节）
      "allowed_types": ["jpg", "jpeg", "png", "gif", "pdf", "doc", "docx"] // 允许的文件类型
    },
    "chat": {
      "max_message_length": 1000,   // 最大消息长度
      "history_retention_days": 365, // 聊天历史保留天数
      "typing_indicator": true,     // 是否显示输入指示器
      "read_receipts": true         // 是否显示已读回执
    }
  }
}
```

### 监控配置 (monitoring)
```json
{
  "monitoring": {
    "metrics_enabled": true,        // 是否启用指标收集
    "health_check_interval": 60000, // 健康检查间隔（毫秒）
    "alert_thresholds": {
      "cpu_usage": 80,              // CPU使用率阈值（%）
      "memory_usage": 85,           // 内存使用率阈值（%）
      "disk_usage": 90,             // 磁盘使用率阈值（%）
      "connection_count": 900       // 连接数阈值
    },
    "alert_email": "admin@qkchat.com" // 告警邮箱
  }
}
```

## 配置修改说明

1. **数据库配置**：修改 `database` 部分来配置MySQL连接
2. **Redis配置**：修改 `redis` 部分来配置Redis连接
3. **SMTP配置**：修改 `smtp` 部分来配置邮件服务
4. **服务器配置**：修改 `server` 部分来配置服务器行为
5. **日志配置**：修改 `logging` 部分来配置日志输出
6. **安全配置**：修改 `security` 部分来配置安全策略
7. **功能配置**：修改 `features` 部分来启用/禁用功能
8. **监控配置**：修改 `monitoring` 部分来配置监控和告警

## 注意事项

1. 修改配置文件后需要重启服务器才能生效
2. 密码等敏感信息建议使用环境变量或加密存储
3. 证书文件路径可以是相对路径或绝对路径
4. 日志目录为空时使用系统默认目录
5. 所有配置项都有默认值，可以根据需要修改