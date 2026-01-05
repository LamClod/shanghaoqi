# ShangHaoQi 上号器 v2.0.0

ShangHaoQi（上号器）是一款基于 Qt6 构建的本地 HTTPS 代理服务器，专为 AI API 请求的拦截、转发和流式处理而设计。通过修改系统 hosts 文件和安装自签名 CA 证书，实现对 api.openai.com、api.anthropic.com 等域名的透明代理，支持 API 密钥注入、模型名称映射和 SSE 流式响应的智能处理。

## 功能特性

- **API 劫持**: 通过 hosts 文件劫持 + 自签名证书实现本地 HTTPS 代理
- **请求转发**: 支持 URL/模型名/API Key 映射转换
- **流式整流**: 流式 ↔ 非流式响应双向转换 (FluxFix 引擎)
- **跨平台**: Windows / Linux / macOS 原生支持

## 架构

```
┌─────────────────────────────────────────────────────────────────┐
│                         UI Layer (Qt6)                          │
│  ┌─────────────┐ ┌──────────────────┐ ┌───────────┐ ┌─────────┐ │
│  │ MainWidget  │ │ConfigGroupPanel  │ │ LogPanel  │ │ Dialog  │ │
│  └─────────────┘ └──────────────────┘ └───────────┘ └─────────┘ │
├─────────────────────────────────────────────────────────────────┤
│                       Core Layer (C++17)                        │
│  ┌────────────────┐ ┌────────────────┐ ┌──────────────────────┐ │
│  │ NetworkManager │ │  LogManager    │ │  FluxFix Wrapper     │ │
│  │ (HTTPS Proxy)  │ │  (Thread-safe) │ │  (Rust FFI Bridge)   │ │
│  └────────────────┘ └────────────────┘ └──────────────────────┘ │
├─────────────────────────────────────────────────────────────────┤
│                    Platform Layer (Abstraction)                 │
│  ┌──────────────┐ ┌──────────────┐ ┌────────────┐ ┌───────────┐ │
│  │ConfigManager │ │ CertManager  │ │HostsManager│ │Privilege  │ │
│  │ (DPAPI/...)  │ │ (OpenSSL)    │ │ (/etc/hosts)│ │Manager    │ │
│  └──────────────┘ └──────────────┘ └────────────┘ └───────────┘ │
├─────────────────────────────────────────────────────────────────┤
│                     FluxFix Engine (Rust cdylib)                │
│  ┌────────────────┐ ┌────────────────┐ ┌──────────────────────┐ │
│  │ StreamAggregator│ │ StreamSplitter │ │ JSON/SSE Fixer      │ │
│  │ (SSE → JSON)   │ │ (JSON → SSE)   │ │ (Malformed Repair)  │ │
│  └────────────────┘ └────────────────┘ └──────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

## 目录结构

```
ShangHaoQi/
├── CMakeLists.txt              # 构建配置
├── src/
│   ├── main.cpp                # 入口
│   ├── platform/               # 平台抽象层
│   │   ├── interfaces/         #   接口定义
│   │   ├── windows/            #   Windows 实现
│   │   ├── linux/              #   Linux 实现
│   │   └── macos/              #   macOS 实现
│   ├── core/                   # 核心业务层
│   │   ├── interfaces/         #   服务接口
│   │   ├── std/                #   标准 C++ 实现
│   │   └── qt/                 #   Qt 实现
│   └── ui/                     # 用户界面层
└── tests/                      # 单元测试
```

## 技术栈

| 层级 | 技术 | 说明 |
|------|------|------|
| UI | Qt 6.x | 跨平台 GUI |
| Core | C++17 | 网络代理、日志 |
| Platform | Win32/POSIX | 系统 API 封装 |
| FluxFix | Rust | 流式整流引擎 |
| 加密 | OpenSSL 3.x | 证书生成/SSL |
| 配置 | yaml-cpp | YAML 序列化 |

## 构建

### 依赖

- CMake 3.20+
- Qt 6.5+
- OpenSSL 3.x
- yaml-cpp
- Rust (用于 FluxFix)

### 编译

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

## 许可证

Copyright (c) 2026 LAMCLOD. All rights reserved.
