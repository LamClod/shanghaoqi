# ShangHaoQi v2.0.0 技术架构文档

## 目录结构

```
proxy-qt/
├── CMakeLists.txt                              # CMake 构建配置 (Qt6 + OpenSSL + yaml-cpp + FluxFix)
├── resources/
│   ├── shanghaoqi_icon.ico                     # 应用图标 (多尺寸 ICO)
│   └── app_icon.rc                             # Windows 资源脚本
│
├── src/
│   ├── main.cpp                                # 入口: QApplication + PlatformFactory + MainWidget
│   │
│   ├── platform/                               # ═══════ 平台抽象层 ═══════
│   │   │
│   │   ├── platform_factory.h/cpp              # 抽象工厂入口
│   │   │   └── createPlatformFactory()         #   → 根据 Q_OS_* 宏返回对应平台工厂
│   │   │
│   │   ├── interfaces/                         # ─────── 平台服务接口 (纯 C++17) ───────
│   │   │   │
│   │   │   ├── i_config_manager.h              # 配置管理器接口
│   │   │   │   ├── AppConfig                   #   配置容器: vector<ProxyConfigItem>
│   │   │   │   ├── load() / save()             #   YAML 序列化 + 敏感字段加密
│   │   │   │   ├── saveAsync()                 #   后台线程异步保存
│   │   │   │   ├── addProxyConfig()            #   CRUD 操作
│   │   │   │   ├── updateProxyConfig()
│   │   │   │   ├── removeProxyConfig()
│   │   │   │   ├── setConfigChangedCallback()  #   变更通知回调
│   │   │   │   └── validateProxyConfig()       #   静态验证: name/localUrl/mappedUrl 非空
│   │   │   │
│   │   │   ├── i_cert_manager.h                # 证书管理器接口
│   │   │   │   ├── generateCACert()            #   X.509 CA 证书 (RSA-2048, SHA-256, 100年有效期)
│   │   │   │   ├── generateServerCert()        #   服务器证书 (CA 签发, SAN 多域名, 1年有效期)
│   │   │   │   ├── generateAllCerts()          #   一键生成 CA + Server
│   │   │   │   ├── installCACert()             #   安装到系统信任链
│   │   │   │   ├── uninstallCACert()           #   从系统信任链移除
│   │   │   │   ├── isCACertInstalled()         #   检查安装状态
│   │   │   │   ├── getCertThumbprint()         #   SHA1 指纹
│   │   │   │   ├── getCertCommonName()         #   证书 CN
│   │   │   │   ├── defaultHijackDomains()      #   默认域名: api.openai.com, api.anthropic.com
│   │   │   │   └── CA_VALIDITY_DAYS = 36500    #   常量: 100年
│   │   │   │       SERVER_VALIDITY_DAYS = 365  #   常量: 1年
│   │   │   │
│   │   │   ├── i_privilege_manager.h           # 权限管理器接口
│   │   │   │   ├── isRunningAsAdmin()          #   检测当前进程权限
│   │   │   │   ├── isUserAdmin()               #   检测用户是否属于管理员组
│   │   │   │   ├── restartAsAdmin()            #   以管理员权限重启应用
│   │   │   │   ├── executeAsAdmin()            #   以管理员权限执行单个命令
│   │   │   │   ├── platformName()              #   "Windows" / "Linux" / "macOS"
│   │   │   │   └── elevationMethod()           #   "UAC" / "pkexec" / "osascript"
│   │   │   │
│   │   │   └── i_hosts_manager.h               # Hosts 管理器接口
│   │   │       ├── addEntry()                  #   添加 127.0.0.1 + ::1 劫持条目
│   │   │       ├── removeEntry()               #   移除劫持条目
│   │   │       ├── hasEntry()                  #   检查条目是否存在
│   │   │       ├── backup() / restore()        #   备份/恢复 hosts 文件
│   │   │       ├── openInEditor()              #   系统编辑器打开
│   │   │       ├── hostsFilePath()             #   系统 hosts 路径
│   │   │       ├── hijackDomains()             #   获取劫持域名列表
│   │   │       └── setHijackDomains()          #   设置劫持域名列表
│   │   │
│   │   ├── windows/                            # ─────── Windows 实现 ───────
│   │   │   ├── windows_platform_factory.h/cpp  #   Windows 工厂
│   │   │   │
│   │   │   ├── windows_config_manager.h/cpp    #   配置管理器
│   │   │   │   ├── 加密: DPAPI (CryptProtectData/CryptUnprotectData)
│   │   │   │   ├── 存储: %APPDATA%/ShangHaoQi/config.yaml
│   │   │   │   └── 敏感字段: authKey, apiKey → Base64(DPAPI(plaintext))
│   │   │   │
│   │   │   ├── windows_cert_manager.h/cpp      #   证书管理器
│   │   │   │   ├── 生成: OpenSSL (EVP_PKEY, X509, X509_EXTENSION)
│   │   │   │   ├── 安装: CryptoAPI (CertOpenSystemStore, CertAddEncodedCertificateToStore)
│   │   │   │   ├── 存储: ROOT (受信任的根证书颁发机构)
│   │   │   │   └── 查询: CertFindCertificateInStore (按 SHA1 指纹)
│   │   │   │
│   │   │   ├── windows_privilege_manager.h/cpp #   权限管理器
│   │   │   │   ├── 检测: OpenProcessToken + GetTokenInformation (TokenElevation)
│   │   │   │   ├── 提权: ShellExecuteEx (lpVerb = "runas")
│   │   │   │   └── 用户组: CheckTokenMembership (DOMAIN_ALIAS_RID_ADMINS)
│   │   │   │
│   │   │   └── windows_hosts_manager.h/cpp     #   Hosts 管理器
│   │   │       ├── 路径: GetSystemDirectory() + "\drivers\etc\hosts"
│   │   │       ├── 备份: %APPDATA%/ShangHaoQi/hosts.backup
│   │   │       ├── 写入: 原子写入 (写临时文件 → MoveFileEx)
│   │   │       └── 编辑器: ShellExecute("notepad.exe", hostsPath)
│   │   │
│   │   ├── linux/                              # ─────── Linux 实现 ───────
│   │   │   ├── linux_platform_factory.h/cpp    #   Linux 工厂
│   │   │   │
│   │   │   ├── linux_config_manager.h/cpp      #   配置管理器
│   │   │   │   ├── 加密: libsecret (Secret Service API) / 降级 Base64
│   │   │   │   ├── 存储: ~/.config/ShangHaoQi/config.yaml
│   │   │   │   └── 条件编译: #ifdef HAVE_LIBSECRET
│   │   │   │
│   │   │   ├── linux_cert_manager.h/cpp        #   证书管理器
│   │   │   │   ├── 生成: OpenSSL
│   │   │   │   ├── 安装: /usr/local/share/ca-certificates/ + update-ca-certificates
│   │   │   │   └── 备选: /etc/pki/ca-trust/source/anchors/ + update-ca-trust
│   │   │   │
│   │   │   ├── linux_privilege_manager.h/cpp   #   权限管理器
│   │   │   │   ├── 检测: getuid() == 0
│   │   │   │   ├── 提权: pkexec / gksudo / kdesudo / sudo (按优先级)
│   │   │   │   └── 用户组: getgroups() + getgrgid()
│   │   │   │
│   │   │   └── linux_hosts_manager.h/cpp       #   Hosts 管理器
│   │   │       ├── 路径: /etc/hosts
│   │   │       ├── 备份: ~/.config/ShangHaoQi/hosts.backup
│   │   │       └── 编辑器: xdg-open / gedit / kate / nano
│   │   │
│   │   └── macos/                              # ─────── macOS 实现 ───────
│   │       ├── macos_platform_factory.h/cpp    #   macOS 工厂
│   │       │
│   │       ├── macos_config_manager.h/cpp      #   配置管理器
│   │       │   ├── 加密: Keychain (SecItemAdd/SecItemCopyMatching)
│   │       │   └── 存储: ~/Library/Application Support/ShangHaoQi/config.yaml
│   │       │
│   │       ├── macos_cert_manager.h/cpp        #   证书管理器
│   │       │   ├── 生成: OpenSSL
│   │       │   ├── 安装: security add-trusted-cert -d -r trustRoot -k /Library/Keychains/System.keychain
│   │       │   └── 查询: security find-certificate -c "ShangHaoQi_CA"
│   │       │
│   │       ├── macos_privilege_manager.h/cpp   #   权限管理器
│   │       │   ├── 检测: getuid() == 0
│   │       │   ├── 提权: osascript -e 'do shell script "..." with administrator privileges'
│   │       │   └── 备选: AuthorizationExecuteWithPrivileges (已废弃但仍可用)
│   │       │
│   │       └── macos_hosts_manager.h/cpp       #   Hosts 管理器
│   │           ├── 路径: /etc/hosts
│   │           ├── 备份: ~/Library/Application Support/ShangHaoQi/hosts.backup
│   │           └── 编辑器: open -e /etc/hosts
│   │
│   ├── core/                                   # ═══════ 核心业务模块 ═══════
│   │   │
│   │   ├── operation_result.h                  # 统一结果类型
│   │   │   ├── ErrorCode 枚举                  #   按模块分组 (0-999)
│   │   │   │   ├── 0         Success
│   │   │   │   ├── 100-199   Port (PortInUse, PortInvalid)
│   │   │   │   ├── 200-299   Cert (CertGenerationFailed, CertInstallFailed, CertNotFound, CertInvalid)
│   │   │   │   ├── 300-399   Hosts (HostsPermissionDenied, HostsWriteFailed, HostsReadFailed, HostsBackupFailed)
│   │   │   │   ├── 400-499   Config (ConfigInvalid, ConfigSaveFailed, ConfigLoadFailed, ConfigNotFound)
│   │   │   │   ├── 500-599   Network (NetworkError, NetworkTimeout, ConnectionFailed)
│   │   │   │   ├── 600-699   Auth (AuthenticationFailed, AuthKeyInvalid, ApiKeyInvalid)
│   │   │   │   ├── 700-799   API (TargetApiError, ApiResponseInvalid)
│   │   │   │   ├── 800-899   Crypto (EncryptionFailed, DecryptionFailed)
│   │   │   │   └── 900-999   General (Unknown)
│   │   │   │
│   │   │   └── OperationResult 结构体
│   │   │       ├── bool ok
│   │   │       ├── string message
│   │   │       ├── ErrorCode code
│   │   │       ├── map<string,string> details
│   │   │       ├── success() / failure()       #   静态工厂方法
│   │   │       ├── isSuccess() / isFailure()
│   │   │       └── operator bool()             #   隐式转换
│   │   │
│   │   ├── interfaces/                         # ─────── 核心服务接口 ───────
│   │   │   │
│   │   │   ├── i_log_manager.h                 # 日志管理器接口
│   │   │   │   ├── LogLevel 枚举               #   Debug(0), Info(1), Warning(2), Error(3)
│   │   │   │   ├── LogCallback                 #   function<void(string, LogLevel)>
│   │   │   │   ├── initialize(logPath)         #   单日志文件初始化
│   │   │   │   ├── initialize(logPath, errPath)#   双日志文件初始化
│   │   │   │   ├── shutdown()                  #   刷新缓冲区 + 关闭文件
│   │   │   │   ├── log() / debug() / info() / warning() / error()
│   │   │   │   ├── setLogCallback()            #   UI 通知回调
│   │   │   │   ├── formatMessage()             #   静态: [HH:MM:SS.mmm] [LEVEL] message
│   │   │   │   ├── timestamp()                 #   静态: HH:MM:SS.mmm
│   │   │   │   └── levelToString()             #   静态: LogLevel → "DEBUG"/"INFO"/...
│   │   │   │
│   │   │   └── i_network_manager.h             # 网络管理器接口 + 数据类型
│   │   │       │
│   │   │       ├── StreamMode 枚举
│   │   │       │   ├── FollowClient = 0        #   跟随客户端请求
│   │   │       │   ├── ForceOn = 1             #   强制流式
│   │   │       │   └── ForceOff = 2            #   强制非流式
│   │   │       │
│   │   │       ├── ProxyConfigItem 结构体
│   │   │       │   ├── name                    #   配置名称
│   │   │       │   ├── localUrl                #   本地 URL (如 https://api.openai.com)
│   │   │       │   ├── mappedUrl               #   映射 URL (如 https://real-api.example.com)
│   │   │       │   ├── localModelName          #   本地模型名 (如 gpt-4)
│   │   │       │   ├── mappedModelName         #   映射模型名 (如 gpt-4-turbo)
│   │   │       │   ├── authKey                 #   本地认证密钥 (客户端使用)
│   │   │       │   ├── apiKey                  #   远程 API 密钥 (转发时使用)
│   │   │       │   └── isValid()               #   验证: name/localUrl/mappedUrl 非空
│   │   │       │
│   │   │       ├── RuntimeOptions 结构体
│   │   │       │   ├── debugMode = false       #   调试模式
│   │   │       │   ├── disableSslStrict = false#   禁用 SSL 严格验证
│   │   │       │   ├── enableHttp2 = true      #   启用 HTTP/2
│   │   │       │   ├── enableConnectionPool = true
│   │   │       │   ├── enableFluxFix = true    #   启用 FluxFix 整流器
│   │   │       │   ├── upstreamStreamMode      #   上游流模式
│   │   │       │   ├── downstreamStreamMode    #   下游流模式
│   │   │       │   ├── proxyPort = 443         #   代理端口
│   │   │       │   ├── connectionPoolSize = 10 #   连接池大小
│   │   │       │   ├── requestTimeout = 120000 #   请求超时 (ms)
│   │   │       │   ├── connectionTimeout = 30000#  连接超时 (ms)
│   │   │       │   └── chunkSize = 20          #   流式分块大小 (字符数)
│   │   │       │
│   │   │       ├── ProxyServerConfig 结构体
│   │   │       │   ├── proxyConfigs             #   vector<ProxyConfigItem>
│   │   │       │   ├── options                  #   RuntimeOptions
│   │   │       │   ├── certPath                 #   服务器证书路径
│   │   │       │   ├── keyPath                  #   服务器私钥路径
│   │   │       │   └── isValid()                #   验证配置完整性
│   │   │       │
│   │   │       ├── StatusCallback               #   function<void(bool running)>
│   │   │       │
│   │   │       └── INetworkManager 接口
│   │   │           ├── start(config)            #   启动代理服务器
│   │   │           ├── stop()                   #   停止代理服务器
│   │   │           ├── isRunning()              #   查询运行状态
│   │   │           ├── config()                 #   获取当前配置
│   │   │           ├── setLogCallback()
│   │   │           ├── setStatusCallback()
│   │   │           ├── MAX_HEADER_SIZE = 16384  #   常量: 16KB
│   │   │           ├── MAX_BODY_SIZE = 10485760 #   常量: 10MB
│   │   │           └── MAX_DEBUG_OUTPUT_LENGTH = 2000
│   │   │
│   │   ├── std/                                # ─────── 标准 C++ 实现 ───────
│   │   │   └── fluxfix_wrapper.h/cpp           # FluxFix C FFI 封装
│   │   │       ├── fluxfix::Response           #   统一响应结构
│   │   │       │   ├── id                      #     响应 ID
│   │   │       │   ├── model                   #     模型名称
│   │   │       │   ├── content                 #     响应内容
│   │   │       │   └── finishReason            #     结束原因
│   │   │       │
│   │   │       ├── fluxfix::Aggregator         #   流式 → 非流式聚合
│   │   │       │   ├── addBytes(data, len)     #     添加 SSE 数据块
│   │   │       │   ├── isComplete()            #     检查流是否完成
│   │   │       │   └── finalize()              #     获取聚合后的响应
│   │   │       │
│   │   │       ├── fluxfix::Splitter           #   非流式 → 流式拆分
│   │   │       │   ├── setChunkSize(size)      #     设置分块大小
│   │   │       │   └── split(id, model, content)#    拆分为 SSE 块数组
│   │   │       │
│   │   │       ├── fluxfix::SseHandler         #   SSE 流处理器
│   │   │       │   ├── addEvent(eventData)     #     添加 SSE 事件
│   │   │       │   ├── isComplete()            #     检查是否完成
│   │   │       │   ├── finalize()              #     获取聚合结果
│   │   │       │   └── reset()                 #     重置处理器状态
│   │   │       │
│   │   │       ├── fluxfix::RectifierTrigger   #   整流触发类型枚举
│   │   │       │   ├── InvalidSignature        #     无效签名
│   │   │       │   ├── MissingThinkingPrefix   #     缺少 thinking 前缀
│   │   │       │   └── InvalidRequest          #     无效请求
│   │   │       │
│   │   │       ├── fluxfix::RectifyResult      #   整流结果结构
│   │   │       │   ├── applied                 #     是否应用整流
│   │   │       │   ├── removedThinkingBlocks   #     移除的 thinking 块数
│   │   │       │   ├── removedRedactedThinkingBlocks
│   │   │       │   ├── removedSignatureFields  #     移除的签名字段数
│   │   │       │   └── removedThinkingField    #     是否移除 thinking 字段
│   │   │       │
│   │   │       ├── fluxfix::Rectifier          #   请求整流器
│   │   │       │   ├── detect(errorMsg)        #     检测错误是否触发整流
│   │   │       │   └── rectify(jsonStr)        #     整流 JSON 请求
│   │   │       │
│   │   │       └── fluxfix::version()          #   获取库版本
│   │   │
│   │   └── qt/                                 # ─────── Qt 框架实现 ───────
│   │       │
│   │       ├── log_manager.h/cpp               # 日志管理器 Qt 实现
│   │       │   ├── 单例模式                    #   LogManager::instance()
│   │       │   ├── 线程安全                    #   QMutex 保护
│   │       │   ├── 文件输出                    #   QFile + QTextStream
│   │       │   ├── 信号槽                      #   logMessage(QString, LogLevel) 信号
│   │       │   └── 宏定义
│   │       │       ├── LOG_DEBUG(msg)
│   │       │       ├── LOG_INFO(msg)
│   │       │       ├── LOG_WARNING(msg)
│   │       │       └── LOG_ERROR(msg)
│   │       │
│   │       ├── network_manager.h/cpp           # 网络管理器 Qt 实现
│   │       │   │
│   │       │   ├── HttpRequest 结构体          #   请求解析结果
│   │       │   │   ├── method / path / httpVersion
│   │       │   │   ├── headers (QMap, 键名小写)
│   │       │   │   ├── body (QByteArray)
│   │       │   │   ├── getHeader() / hasHeader()
│   │       │   │   └── shouldKeepAlive()       #   HTTP/1.1 默认 keep-alive
│   │       │   │
│   │       │   ├── ConnectionPool 类           #   连接池管理
│   │       │   │   ├── QQueue<QNetworkAccessManager*> m_pool
│   │       │   │   ├── QSet<QNetworkAccessManager*> m_inUse
│   │       │   │   ├── acquire(enableHttp2)    #   获取连接 (池空则新建)
│   │       │   │   ├── release(manager)        #   归还连接 (池满则销毁)
│   │       │   │   └── clear()                 #   清空所有连接
│   │       │   │
│   │       │   └── NetworkManager 类           #   HTTPS 代理服务器
│   │       │       ├── QSslServer* m_server    #   SSL 服务器
│   │       │       ├── ConnectionPool          #   连接池
│   │       │       ├── QMap<QSslSocket*, QByteArray> m_pendingData  # 待处理数据
│   │       │       ├── QMap<QNetworkReply*, QSslSocket*> m_replyToSocket
│   │       │       ├── unordered_map<QNetworkReply*, FluxFixSseHandler> m_sseHandlers
│   │       │       │
│   │       │       ├── start(config)           #   配置 SSL + 监听端口
│   │       │       ├── stop()                  #   关闭服务器 + 清理连接
│   │       │       ├── isPortInUse(port)       #   静态: 端口占用检测
│   │       │       │
│   │       │       ├── 请求处理流程
│   │       │       │   ├── onNewConnection()   #   新连接 → 配置 SSL
│   │       │       │   ├── onSocketReadyRead() #   数据到达 → 解析请求
│   │       │       │   ├── parseHttpRequest()  #   HTTP 请求解析
│   │       │       │   ├── handleRequest()     #   路由分发
│   │       │       │   ├── handleModelsRequest()#  GET /v1/models
│   │       │       │   ├── handleChatCompletionsRequest()  # POST /v1/chat/completions
│   │       │       │   └── handleUnknownRequest()#  404 响应
│   │       │       │
│   │       │       ├── 认证处理
│   │       │       │   ├── verifyAuth()        #   验证 Authorization / x-api-key
│   │       │       │   └── sendUnauthorizedResponse()  # 401 响应
│   │       │       │
│   │       │       ├── 请求转发
│   │       │       │   ├── forwardRequest()    #   构建上游请求
│   │       │       │   └── transformRequestBody()#  模型名称替换 + API 密钥注入
│   │       │       │
│   │       │       ├── 响应处理
│   │       │       │   ├── handleStreamingResponse()      # SSE 流式响应
│   │       │       │   ├── handleNonStreamingResponse()   # 普通 JSON 响应
│   │       │       │   ├── handleNonStreamToStreamResponse()# 非流式→流式转换
│   │       │       │   ├── convertStreamToNonStream()     # 流式→非流式转换
│   │       │       │   ├── convertStreamToNonStreamSimple()# 简化版流式→非流式
│   │       │       │   ├── splitAndSendSimple()           # 简化版响应拆分发送
│   │       │       │   ├── normalizeNonStreamResponse()   # 规范化非流式响应
│   │       │       │   └── forwardErrorResponse()         # 错误响应转发
│   │       │       │
│   │       │       ├── HTTP 响应工具
│   │       │       │   ├── sendHttpResponse()             # 完整响应
│   │       │       │   ├── sendHttpResponseHeaders()      # 仅头部
│   │       │       │   └── sendSseChunk()                 # SSE 数据块
│   │       │       │
│   │       │       └── 信号
│   │       │           ├── logMessage(QString, LogLevel)
│   │       │           └── statusChanged(bool running)
│   │       │
│   │       └── fluxfix_wrapper.h/cpp           # FluxFix Qt 适配层
│   │           ├── FluxFixAggregator           #   QByteArray 支持
│   │           ├── FluxFixSplitter             #   QString 支持
│   │           └── FluxFixSseHandler           #   Qt 类型适配
│   │
│   └── ui/                                     # ═══════ 用户界面层 ═══════
│       │
│       ├── main_widget.h/cpp                   # 主窗口
│       │   ├── 依赖注入                        #   MainWidget(IPlatformFactoryPtr)
│       │   ├── 平台服务成员
│       │   │   ├── m_platformFactory
│       │   │   ├── m_configManager
│       │   │   ├── m_certManager
│       │   │   ├── m_privilegeManager
│       │   │   └── m_hostsManager
│       │   │
│       │   ├── 核心服务成员
│       │   │   └── m_proxyServer (NetworkManager*)
│       │   │
│       │   ├── UI 组件
│       │   │   ├── m_stackedWidget             #   页面切换
│       │   │   ├── m_configGroupPanel          #   配置面板
│       │   │   ├── m_runtimeOptionsPanel       #   运行时选项面板
│       │   │   ├── m_logPanel                  #   日志面板
│       │   │   ├── m_statusLabel               #   状态标签
│       │   │   ├── m_startAllButton            #   一键启动按钮
│       │   │   ├── m_stopButton                #   停止按钮
│       │   │   └── m_toggleViewButton          #   视图切换按钮
│       │   │
│       │   ├── 一键启动流程 (onStartAllServices)
│       │   │   ├── 1. 验证配置                 #   检查 ProxyConfigItem 有效性
│       │   │   ├── 2. 生成证书                 #   generateAllCerts()
│       │   │   ├── 3. 安装证书                 #   installCACert()
│       │   │   ├── 4. 修改 hosts               #   addEntry()
│       │   │   └── 5. 启动代理                 #   m_proxyServer->start()
│       │   │
│       │   ├── 停止流程 (onStopServices)
│       │   │   ├── 1. 停止代理                 #   m_proxyServer->stop()
│       │   │   └── 2. 恢复 hosts               #   removeEntry()
│       │   │
│       │   └── closeEvent()                    #   窗口关闭时自动 cleanup()
│       │
│       ├── config_group_panel.h/cpp            # 配置面板
│       │   ├── 配置列表 (QListWidget)
│       │   ├── 配置编辑表单
│       │   │   ├── 名称 / 本地URL / 映射URL
│       │   │   ├── 本地模型名 / 映射模型名
│       │   │   └── 认证密钥 / API密钥
│       │   ├── CRUD 操作
│       │   │   ├── 添加 / 编辑 / 删除
│       │   │   └── 导入 / 导出 (YAML)
│       │   └── 测活功能
│       │       ├── 单个测活
│       │       └── 批量测活
│       │
│       ├── runtime_options_panel.h/cpp         # 运行时选项面板
│       │   ├── 代理端口 (QSpinBox)
│       │   ├── 调试模式 (QCheckBox)
│       │   ├── 启用 FluxFix 整流器 (QCheckBox)
│       │   ├── 上游流模式 (QComboBox)
│       │   ├── 下游流模式 (QComboBox)
│       │   ├── 请求超时 (QSpinBox)
│       │   ├── 连接超时 (QSpinBox)
│       │   ├── 连接池大小 (QSpinBox)
│       │   └── 分块大小 (QSpinBox)
│       │
│       ├── log_panel.h/cpp                     # 日志面板
│       │   ├── QTextEdit (只读, 深色主题)
│       │   ├── 颜色编码
│       │   │   ├── Debug: 灰色
│       │   │   ├── Info: 白色
│       │   │   ├── Warning: 黄色
│       │   │   └── Error: 红色
│       │   ├── 自动滚动
│       │   └── 清空按钮
│       │
│       └── test_result_dialog.h/cpp            # 测活结果对话框
│           ├── 单个测活结果
│           │   ├── 状态 (成功/失败)
│           │   ├── 响应时间
│           │   └── 错误信息
│           └── 批量测活结果
│               ├── 成功数 / 失败数
│               └── 详细列表
│
└── tests/                                      # ═══════ 单元测试 ═══════
    ├── CMakeLists.txt                          # Qt Test 配置
    ├── test_config_manager.cpp                 # 配置管理器测试
    ├── test_hosts_manager.cpp                  # Hosts 管理器测试
    ├── test_log_manager.cpp                    # 日志管理器测试
    ├── test_network_utils.cpp                  # 网络工具测试
    └── test_platform_factory.cpp               # 平台工厂测试

FluxFix/                                        # ═══════ 整流中间件库 (Rust cdylib) ═══════
├── Cargo.toml                                  # 依赖: bytes, serde, simd-json, thiserror, async-trait, memchr
├── build.rs                                    # 构建脚本: cbindgen 生成 C 头文件
├── cbindgen.toml                               # cbindgen 配置
│
├── include/
│   └── fluxfix.h                               # C 头文件 (cbindgen 自动生成)
│       ├── FFIStreamAggregator                 #   流式聚合器句柄
│       ├── FFIStreamSplitter                   #   流式拆分器句柄
│       ├── FFIResponse                         #   响应结构 (id, model, content, finish_reason)
│       ├── FFIBuffer                           #   字节缓冲区 (data, len, capacity)
│       ├── FFIChunkArray                       #   块数组 (chunks, len, capacity)
│       │
│       ├── fluxfix_aggregator_new()            #   创建聚合器
│       ├── fluxfix_aggregator_free()           #   释放聚合器
│       ├── fluxfix_aggregator_add_bytes()      #   添加数据块
│       ├── fluxfix_aggregator_is_complete()    #   检查完成状态
│       ├── fluxfix_aggregator_finalize()       #   获取聚合结果
│       ├── fluxfix_response_free()             #   释放响应
│       │
│       ├── fluxfix_splitter_new()              #   创建拆分器
│       ├── fluxfix_splitter_free()             #   释放拆分器
│       ├── fluxfix_splitter_set_chunk_size()   #   设置分块大小
│       ├── fluxfix_splitter_split()            #   执行拆分
│       │
│       ├── fluxfix_chunk_array_free()          #   释放块数组
│       ├── fluxfix_chunk_array_len()           #   获取块数量
│       ├── fluxfix_chunk_array_get()           #   获取指定块
│       │
│       └── fluxfix_version()                   #   获取版本号
│
├── src/
│   ├── lib.rs                                  # 库入口: 模块声明 + 重导出
│   │
│   ├── ffi.rs                                  # FFI 导出层
│   │   ├── FFIStreamAggregator                 #   聚合器 FFI 包装
│   │   ├── FFIStreamSplitter                   #   拆分器 FFI 包装
│   │   ├── FFIBuffer / FFIChunkArray / FFIResponse
│   │   └── extern "C" fn fluxfix_*             #   C ABI 导出函数
│   │
│   ├── config/                                 # 配置模块
│   │   └── mod.rs
│   │       ├── FluxFixConfig                   #   主配置
│   │       ├── ServerConfig                    #   服务器配置
│   │       ├── FixerConfig                     #   修复器配置
│   │       └── ObservabilityConfig             #   可观测性配置
│   │
│   ├── domain/                                 # ─────── 领域模型 ───────
│   │   ├── mod.rs                              #   模块声明 + 重导出
│   │   │
│   │   ├── models/                             #   数据模型
│   │   │   ├── mod.rs
│   │   │   │
│   │   │   ├── request.rs                      #   请求模型
│   │   │   │   ├── UnifiedRequest              #     统一请求格式
│   │   │   │   │   ├── model: String
│   │   │   │   │   ├── messages: Vec<Message>
│   │   │   │   │   ├── stream: bool
│   │   │   │   │   ├── max_tokens: Option<u32>
│   │   │   │   │   ├── temperature: Option<f32>
│   │   │   │   │   ├── top_p: Option<f32>
│   │   │   │   │   ├── stop: Option<Vec<String>>
│   │   │   │   │   └── extra: HashMap<String, Value>
│   │   │   │   │
│   │   │   │   ├── Message                     #     消息结构
│   │   │   │   │   ├── role: String
│   │   │   │   │   └── content: MessageContent
│   │   │   │   │
│   │   │   │   ├── MessageContent              #     消息内容 (支持多模态)
│   │   │   │   │   ├── Text(String)
│   │   │   │   │   └── Parts(Vec<ContentPart>)
│   │   │   │   │
│   │   │   │   └── ContentPart                 #     内容部分
│   │   │   │       ├── Text { text }
│   │   │   │       └── Image { source: ImageSource }
│   │   │   │
│   │   │   ├── response.rs                     #   响应模型
│   │   │   │   ├── UnifiedResponse             #     统一响应格式
│   │   │   │   │   ├── id: String
│   │   │   │   │   ├── model: String
│   │   │   │   │   ├── choices: Vec<Choice>
│   │   │   │   │   ├── usage: Option<Usage>
│   │   │   │   │   └── extra: HashMap<String, Value>
│   │   │   │   │
│   │   │   │   ├── Choice                      #     选择结构
│   │   │   │   │   ├── index: u32
│   │   │   │   │   ├── message: ResponseMessage
│   │   │   │   │   └── finish_reason: Option<String>
│   │   │   │   │
│   │   │   │   ├── ResponseMessage             #     响应消息
│   │   │   │   │   ├── role: String
│   │   │   │   │   └── content: ResponseContent
│   │   │   │   │
│   │   │   │   ├── ResponseContent             #     响应内容 (支持多模态)
│   │   │   │   │   ├── Text(String)
│   │   │   │   │   └── Blocks(Vec<ResponseContentBlock>)
│   │   │   │   │
│   │   │   │   ├── ResponseContentBlock        #     响应内容块
│   │   │   │   │   ├── Text { text }
│   │   │   │   │   └── ToolUse { id, name, input }
│   │   │   │   │
│   │   │   │   └── Usage                       #     使用量统计
│   │   │   │       ├── prompt_tokens: u32
│   │   │   │       ├── completion_tokens: u32
│   │   │   │       └── total_tokens: u32
│   │   │   │
│   │   │   └── stream.rs                       #   流式数据模型
│   │   │       ├── StreamChunk                 #     流式数据块
│   │   │       │   ├── data: Bytes
│   │   │       │   ├── is_complete: bool
│   │   │       │   └── chunk_type: ChunkType
│   │   │       │
│   │   │       ├── ChunkType                   #     块类型枚举
│   │   │       │   ├── SSE                     #       Server-Sent Events
│   │   │       │   ├── JSON                    #       JSON 格式
│   │   │       │   └── Raw                     #       原始字节
│   │   │       │
│   │   │       ├── StreamResponse              #     流式响应
│   │   │       ├── StreamChoice                #     流式选择
│   │   │       └── Delta                       #     增量内容
│   │   │           ├── role: Option<String>
│   │   │           └── content: Option<String>
│   │   │
│   │   ├── converter/                          #   转换器
│   │   │   ├── mod.rs
│   │   │   │   ├── ConverterError              #     转换错误枚举
│   │   │   │   │   ├── AggregationError
│   │   │   │   │   ├── SplitError
│   │   │   │   │   ├── InvalidChunk
│   │   │   │   │   └── IncompleteStream
│   │   │   │   └── ConverterResult<T>          #     Result 别名
│   │   │   │
│   │   │   ├── aggregator.rs                   #   流式聚合器 (流式 → 非流式)
│   │   │   │   └── StreamAggregator
│   │   │   │       ├── 状态字段
│   │   │   │       │   ├── id: Option<String>
│   │   │   │       │   ├── model: Option<String>
│   │   │   │       │   ├── role: Option<String>
│   │   │   │       │   ├── content_buffer: String
│   │   │   │       │   ├── finish_reason: Option<String>
│   │   │   │       │   ├── usage: Option<Usage>
│   │   │   │       │   ├── is_done: bool
│   │   │   │       │   └── chunks_received: usize
│   │   │   │       │
│   │   │   │       ├── new()                   #     创建聚合器
│   │   │   │       ├── add_chunk(StreamChunk)  #     添加数据块
│   │   │   │       ├── add_bytes(&[u8])        #     添加原始字节
│   │   │   │       ├── finalize() → UnifiedResponse  # 获取聚合结果
│   │   │   │       ├── is_complete()           #     检查完成状态
│   │   │   │       ├── chunks_received()       #     已接收块数
│   │   │   │       └── current_content()       #     当前内容
│   │   │   │
│   │   │   │       ├── 内部方法
│   │   │   │       │   ├── process_sse_chunk() #     处理 SSE 格式
│   │   │   │       │   ├── process_json_chunk()#     处理 JSON 格式
│   │   │   │       │   ├── process_raw_chunk() #     处理原始字节
│   │   │   │       │   └── parse_stream_json() #     解析流式 JSON (simd-json)
│   │   │   │
│   │   │   └── splitter.rs                     #   流式拆分器 (非流式 → 流式)
│   │   │       └── StreamSplitter
│   │   │           ├── 配置字段
│   │   │           │   ├── chunk_size: usize = 20
│   │   │           │   ├── include_role_chunk: bool = true
│   │   │           │   └── include_done_marker: bool = true
│   │   │           │
│   │   │           ├── new()                   #     创建拆分器
│   │   │           ├── with_chunk_size(size)   #     设置分块大小
│   │   │           ├── with_role_chunk(bool)   #     是否包含角色块
│   │   │           ├── with_done_marker(bool)  #     是否包含 [DONE] 标记
│   │   │           ├── split(UnifiedResponse) → Vec<StreamChunk>
│   │   │           ├── split_iter(UnifiedResponse) → Iterator
│   │   │           └── split_to_sse(UnifiedResponse) → Vec<Bytes>
│   │   │
│   │   │           ├── 内部结构
│   │   │           │   ├── SSEChunkData        #     SSE 块数据
│   │   │           │   ├── SSEChoice           #     SSE 选择
│   │   │           │   ├── SSEDelta            #     SSE 增量
│   │   │           │   └── StreamChunkIterator #     块迭代器
│   │   │           │       ├── IteratorState   #       状态机
│   │   │           │       │   ├── RoleChunk
│   │   │           │       │   ├── ContentChunks
│   │   │           │       │   ├── FinishChunk
│   │   │           │       │   ├── DoneMarker
│   │   │           │       │   └── Finished
│   │   │
│   │   ├── fixer/                              #   修复器
│   │   │   ├── mod.rs
│   │   │   │   ├── FixerError                  #     修复错误枚举
│   │   │   │   │   ├── InvalidJson
│   │   │   │   │   ├── InvalidSSE
│   │   │   │   │   ├── EncodingError
│   │   │   │   │   └── ChunkAssemblyError
│   │   │   │   │
│   │   │   │   ├── FixerResult<T>              #     Result 别名
│   │   │   │   │
│   │   │   │   └── trait Fixer                 #     修复器特征
│   │   │   │       ├── fix(&[u8]) → Bytes      #       执行修复
│   │   │   │       └── can_fix(&[u8]) → bool   #       快速检测
│   │   │   │
│   │   │   ├── json_fixer.rs                   #   JSON 修复器
│   │   │   │   └── JsonFixer
│   │   │   │       ├── max_depth: usize = 200  #     最大嵌套深度
│   │   │   │       ├── max_size: usize = 1MB   #     最大输入大小
│   │   │   │       │
│   │   │   │       ├── 修复能力
│   │   │   │       │   ├── 未闭合的 {} []      #       自动闭合
│   │   │   │       │   ├── 未闭合的字符串      #       添加 "
│   │   │   │       │   ├── 尾随逗号            #       移除
│   │   │   │       │   ├── 不完整的转义序列    #       移除末尾 \
│   │   │   │       │   └── 冒号后缺少值        #       添加 null
│   │   │   │       │
│   │   │   │       ├── repair(&[u8]) → Vec<u8> #     修复算法
│   │   │   │       │   ├── 栈追踪括号类型
│   │   │   │       │   ├── 字符串状态机
│   │   │   │       │   └── 转义序列处理
│   │   │   │       │
│   │   │   │       └── validate(&mut [u8]) → bool  # simd-json 验证
│   │   │   │
│   │   │   ├── sse_fixer.rs                    #   SSE 修复器
│   │   │   │   └── SSEFixer
│   │   │   │       ├── auto_prefix_json: bool = true
│   │   │   │       ├── normalize_line_endings: bool = true
│   │   │   │       ├── ensure_termination: bool = true
│   │   │   │       │
│   │   │   │       ├── 修复能力
│   │   │   │       │   ├── 缺失 "data: " 前缀  #       自动添加
│   │   │   │       │   ├── "data:" 后缺空格    #       添加空格
│   │   │   │       │   ├── 换行符不统一        #       CR/CRLF → LF
│   │   │   │       │   ├── 大小写错误          #       Data:/DATA: → data:
│   │   │   │       │   ├── 冒号前有空格        #       "data :" → "data:"
│   │   │   │       │   └── 连续空行            #       合并
│   │   │   │       │
│   │   │   │       ├── fix_sse(&[u8]) → Vec<u8>
│   │   │   │       ├── fix_line(&[u8]) → Vec<u8>
│   │   │   │       ├── fix_data_line(&[u8]) → Vec<u8>
│   │   │   │       ├── looks_like_json(&[u8]) → bool
│   │   │   │       └── try_fix_malformed(&[u8]) → Option<Vec<u8>>
│   │   │   │
│   │   │   │       └── SseLineIterator         #     行迭代器 (处理 CR/LF/CRLF)
│   │   │   │
│   │   │   ├── chunk_assembler.rs              #   分块重组器
│   │   │   │   └── ChunkAssembler
│   │   │   │       ├── buffer: VecDeque<ChunkFragment>
│   │   │   │       ├── max_buffer_size: usize = 100
│   │   │   │       ├── timeout: Duration = 30s
│   │   │   │       ├── 统计计数器 (AtomicU64)
│   │   │   │       │
│   │   │   │       ├── add_chunk(data, sequence)#    添加数据块
│   │   │   │       ├── try_assemble() → Option<Vec<u8>>  # 尝试重组
│   │   │   │       ├── extract_complete() → Vec<Vec<u8>> # 提取完整消息
│   │   │   │       ├── stats() → ChunkAssemblerStats
│   │   │   │       │
│   │   │   │       ├── 边界检测
│   │   │   │       │   ├── find_message_end()  #       查找消息结束
│   │   │   │       │   ├── find_sse_boundary() #       查找 \n\n
│   │   │   │       │   ├── find_json_end()     #       查找平衡括号
│   │   │   │       │   └── is_json_complete()  #       括号平衡检测
│   │   │   │       │
│   │   │   │       └── cleanup_old_chunks()    #     超时清理
│   │   │   │
│   │   │   └── encoding_fixer.rs               #   编码修复器
│   │   │       └── EncodingFixer
│   │   │           ├── BOM 移除
│   │   │           ├── 空字节移除
│   │   │           └── UTF-8 验证/修复
│   │   │
│   │   └── validator/                          #   验证器
│   │       └── mod.rs
│   │           ├── ValidationError             #     验证错误枚举
│   │           │   ├── MissingField(String)
│   │           │   ├── InvalidValue { field, reason }
│   │           │   └── SchemaMismatch(String)
│   │           │
│   │           ├── ValidationResult<T>         #     Result 别名
│   │           │
│   │           ├── trait Validator             #     验证器特征
│   │           │   ├── type Input
│   │           │   └── validate(&Input) → ValidationResult<()>
│   │           │
│   │           └── quick 模块                  #     快速验证辅助函数
│   │               ├── is_chat_completion_request()  # 检查 messages + model 字段
│   │               ├── is_sse_data()           #     检查 data:/event:/: 前缀
│   │               └── is_json_like()          #     检查 { 或 [ 开头
│   │
│   ├── adapters/                               # ─────── 适配器层 ───────
│   │   ├── mod.rs
│   │   │
│   │   ├── middleware.rs                       #   中间件
│   │   │   └── FluxFixMiddleware
│   │   │       ├── 请求拦截链
│   │   │       └── 响应拦截链
│   │   │
│   │   └── providers/                          #   API 提供商适配器
│   │       ├── mod.rs
│   │       │
│   │       ├── openai.rs                       #   OpenAI 适配器
│   │       │   └── OpenAIProvider
│   │       │       ├── sse_fixer: SSEFixer
│   │       │       ├── json_fixer: JsonFixer
│   │       │       │
│   │       │       ├── 请求结构
│   │       │       │   ├── OpenAIRequest
│   │       │       │   │   ├── model, messages, stream
│   │       │       │   │   ├── max_tokens, temperature, top_p
│   │       │       │   │   ├── stop, n, presence_penalty, frequency_penalty
│   │       │       │   │   ├── logit_bias, user, seed
│   │       │       │   │   ├── response_format (JSON mode)
│   │       │       │   │   ├── tools (Function calling)
│   │       │       │   │   └── tool_choice
│   │       │       │   │
│   │       │       │   ├── OpenAIMessage       #     支持多模态
│   │       │       │   │   ├── role, content
│   │       │       │   │   ├── name, tool_calls, tool_call_id
│   │       │       │   │   └── MessageContent
│   │       │       │   │       ├── Text(String)
│   │       │       │   │       └── Parts(Vec<ContentPart>)
│   │       │       │   │           ├── Text { text }
│   │       │       │   │           └── ImageUrl { image_url: { url, detail } }
│   │       │       │   │
│   │       │       │   └── Tool / FunctionDefinition / ToolChoice
│   │       │       │
│   │       │       ├── 响应结构
│   │       │       │   ├── OpenAIResponse      #     非流式响应
│   │       │       │   │   ├── id, object, created, model
│   │       │       │   │   ├── choices: Vec<OpenAIChoice>
│   │       │       │   │   ├── usage: OpenAIUsage
│   │       │       │   │   └── system_fingerprint
│   │       │       │   │
│   │       │       │   └── OpenAIStreamResponse#     流式响应
│   │       │       │       ├── id, object, created, model
│   │       │       │       ├── choices: Vec<OpenAIStreamChoice>
│   │       │       │       │   └── delta: OpenAIDelta
│   │       │       │       │       ├── role, content
│   │       │       │       │       └── tool_calls
│   │       │       │       └── system_fingerprint
│   │       │       │
│   │       │       ├── Provider trait 实现
│   │       │       │   ├── parse_request()     #     OpenAI → Unified
│   │       │       │   ├── format_response()   #     Unified → OpenAI
│   │       │       │   ├── fix_stream_chunk()  #     SSE + JSON 修复
│   │       │       │   ├── name() → "openai"
│   │       │       │   ├── can_handle_path()   #     /v1/chat/completions, /v1/completions
│   │       │       │   └── can_handle_body()   #     尝试解析为 OpenAI 格式
│   │       │       │
│   │       │       └── 内部方法
│   │       │           ├── parse_openai_request()
│   │       │           ├── to_unified_request()
│   │       │           ├── from_unified_response()
│   │       │           └── parse_stream_chunk()
│   │       │
│   │       └── anthropic.rs                    #   Anthropic 适配器
│   │           └── AnthropicProvider
│   │               ├── 请求结构
│   │               │   └── AnthropicRequest
│   │               │       ├── model, messages, max_tokens
│   │               │       ├── system, temperature, top_p
│   │               │       └── stream
│   │               │
│   │               ├── 响应结构
│   │               │   ├── AnthropicResponse
│   │               │   └── AnthropicStreamResponse
│   │               │       ├── event types
│   │               │       │   ├── message_start
│   │               │       │   ├── content_block_start
│   │               │       │   ├── content_block_delta
│   │               │       │   ├── content_block_stop
│   │               │       │   └── message_stop
│   │               │
│   │               └── Provider trait 实现
│   │                   └── can_handle_path()   #     /v1/messages
│   │
│   ├── ports/                                  # ─────── 端口层 ───────
│   │   ├── mod.rs
│   │   │   ├── ProviderError                   #     提供商错误枚举
│   │   │   │   ├── ParseError
│   │   │   │   ├── FormatError
│   │   │   │   └── UnsupportedFormat
│   │   │   │
│   │   │   ├── ProviderResult<T>               #     Result 别名
│   │   │   │
│   │   │   └── paths 模块                      #     路径匹配
│   │   │       ├── is_openai_path()
│   │   │       └── is_anthropic_path()
│   │   │
│   │   └── provider.rs                         #   Provider trait
│   │       └── trait Provider: Send + Sync
│   │           ├── parse_request(&[u8]) → UnifiedRequest
│   │           ├── format_response(&UnifiedResponse) → Bytes
│   │           ├── fix_stream_chunk(&[u8]) → StreamChunk
│   │           ├── name() → &str
│   │           ├── can_handle_path(&str) → bool
│   │           └── can_handle_body(&[u8]) → bool
│   │
│   └── observability/                          # ─────── 可观测性 ───────
│       ├── mod.rs
│       │
│       ├── metrics.rs                          #   指标收集
│       │   └── Metrics
│       │       ├── requests_total
│       │       ├── requests_duration
│       │       ├── chunks_processed
│       │       └── errors_total
│       │
│       └── tracing.rs                          #   链路追踪
│           └── Tracing
│               ├── span 管理
│               └── 日志集成
│
└── tests/
    └── integration_tests.rs                    # 集成测试
```


## 架构图

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                                    UI Layer (Qt Widgets)                                │
│  ┌─────────────────┐  ┌─────────────────────┐  ┌─────────────────┐  ┌───────────────┐  │
│  │   MainWidget    │  │  ConfigGroupPanel   │  │    LogPanel     │  │RuntimeOptions │  │
│  │  ─────────────  │  │  ─────────────────  │  │  ─────────────  │  │  ───────────  │  │
│  │ • 服务编排      │  │ • YAML CRUD         │  │ • 实时日志      │  │ • 端口配置    │  │
│  │ • 生命周期管理  │  │ • 导入/导出         │  │ • 颜色编码      │  │ • 超时设置    │  │
│  │ • 一键启动/停止 │  │ • 测活功能          │  │ • 自动滚动      │  │ • 流模式      │  │
│  └────────┬────────┘  └─────────────────────┘  └─────────────────┘  └───────────────┘  │
└───────────┼─────────────────────────────────────────────────────────────────────────────┘
            │ 依赖注入 (IPlatformFactoryPtr)
            ▼
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                              Interface Layer (Pure C++17)                               │
│  ┌─────────────────────────────────────────────────────────────────────────────────┐   │
│  │                           Platform Interfaces                                    │   │
│  │  ┌─────────────────┐ ┌─────────────────┐ ┌──────────────────┐ ┌───────────────┐ │   │
│  │  │ IConfigManager  │ │  ICertManager   │ │IPrivilegeManager │ │ IHostsManager │ │   │
│  │  │ ─────────────── │ │ ─────────────── │ │ ──────────────── │ │ ───────────── │ │   │
│  │  │ • load/save     │ │ • generateCA    │ │ • isRunningAdmin │ │ • addEntry    │ │   │
│  │  │ • CRUD          │ │ • generateServer│ │ • restartAsAdmin │ │ • removeEntry │ │   │
│  │  │ • encrypt/decrypt│ │ • install/uninstall│ │ • executeAsAdmin │ │ • backup/restore│ │   │
│  │  └─────────────────┘ └─────────────────┘ └──────────────────┘ └───────────────┘ │   │
│  └─────────────────────────────────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────────────────────────────────┐   │
│  │                             Core Interfaces                                      │   │
│  │  ┌─────────────────────────────────┐  ┌─────────────────────────────────────┐   │   │
│  │  │         ILogManager             │  │          INetworkManager            │   │   │
│  │  │  ───────────────────────────    │  │  ─────────────────────────────────  │   │   │
│  │  │  • LogLevel (Debug/Info/Warn/Err)│  │  • ProxyConfigItem                 │   │   │
│  │  │  • LogCallback                  │  │  • RuntimeOptions                   │   │   │
│  │  │  • initialize/shutdown          │  │  • ProxyServerConfig                │   │   │
│  │  │  • log/debug/info/warning/error │  │  • start/stop/isRunning             │   │   │
│  │  └─────────────────────────────────┘  └─────────────────────────────────────┘   │   │
│  └─────────────────────────────────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────────────────────────────────┐   │
│  │                             OperationResult                                      │   │
│  │  ErrorCode: 0=Success | 100-199=Port | 200-299=Cert | 300-399=Hosts | ...       │   │
│  └─────────────────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────────────────┘
            │
            ├──────────────────────────────────────────────────────────────┐
            ▼                                                              ▼
┌───────────────────────────────────┐  ┌───────────────────────────────────────────────┐
│     Platform Implementations      │  │           Core Implementations                │
│  ┌─────────────────────────────┐  │  │  ┌─────────────────────────────────────────┐  │
│  │         Windows             │  │  │  │              Qt Implementation          │  │
│  │  ───────────────────────    │  │  │  │  ─────────────────────────────────────  │  │
│  │  • DPAPI (CryptProtectData) │  │  │  │  ┌─────────────────────────────────┐    │  │
│  │  • CryptoAPI (CertStore)    │  │  │  │  │        NetworkManager           │    │  │
│  │  • UAC (ShellExecuteEx)     │  │  │  │  │  ─────────────────────────────  │    │  │
│  │  • %SystemRoot%\...\hosts   │  │  │  │  │  • QSslServer (TLS 1.2/1.3)    │    │  │
│  └─────────────────────────────┘  │  │  │  │  • ConnectionPool               │    │  │
│  ┌─────────────────────────────┐  │  │  │  │  • HTTP 请求解析                │    │  │
│  │          Linux              │  │  │  │  │  • 请求转发 (QNetworkAccessMgr) │    │  │
│  │  ───────────────────────    │  │  │  │  │  • SSE 流式响应处理             │    │  │
│  │  • libsecret / Base64       │  │  │  │  │  • 流模式转换                   │    │  │
│  │  • update-ca-certificates   │  │  │  │  └─────────────────────────────────┘    │  │
│  │  • pkexec / sudo            │  │  │  │  ┌─────────────────────────────────┐    │  │
│  │  • /etc/hosts               │  │  │  │  │         LogManager              │    │  │
│  └─────────────────────────────┘  │  │  │  │  ─────────────────────────────  │    │  │
│  ┌─────────────────────────────┐  │  │  │  │  • Singleton + QMutex           │    │  │
│  │          macOS              │  │  │  │  │  • QFile + QTextStream          │    │  │
│  │  ───────────────────────    │  │  │  │  │  • Signal/Slot 通知             │    │  │
│  │  • Keychain (SecItem*)      │  │  │  │  └─────────────────────────────────┘    │  │
│  │  • security add-trusted-cert│  │  │  └─────────────────────────────────────────┘  │
│  │  • osascript                │  │  └───────────────────────────────────────────────┘
│  │  • /etc/hosts               │  │
│  └─────────────────────────────┘  │
└───────────────────────────────────┘
            │
            │ FFI (C ABI)
            ▼
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                            FluxFix (Rust cdylib)                                        │
│  ┌─────────────────────────────────────────────────────────────────────────────────┐   │
│  │                              FFI Layer                                           │   │
│  │  fluxfix_aggregator_* | fluxfix_splitter_* | fluxfix_chunk_array_* | fluxfix_version│
│  └─────────────────────────────────────────────────────────────────────────────────┘   │
│                                        │                                                │
│  ┌─────────────────────────────────────┼───────────────────────────────────────────┐   │
│  │                              Domain Layer                                        │   │
│  │  ┌───────────────────┐  ┌───────────────────┐  ┌───────────────────────────┐    │   │
│  │  │      Models       │  │    Converters     │  │         Fixers            │    │   │
│  │  │  ───────────────  │  │  ───────────────  │  │  ─────────────────────    │    │   │
│  │  │ • UnifiedRequest  │  │ • StreamAggregator│  │ • JsonFixer               │    │   │
│  │  │ • UnifiedResponse │  │   (流式→非流式)   │  │   - 未闭合括号修复        │    │   │
│  │  │ • StreamChunk     │  │ • StreamSplitter  │  │   - 尾随逗号移除          │    │   │
│  │  │ • ChunkType       │  │   (非流式→流式)   │  │   - simd-json 验证        │    │   │
│  │  │ • Message         │  │ • 状态机迭代器    │  │ • SSEFixer                │    │   │
│  │  │ • ResponseContent │  │                   │  │   - data: 前缀修复        │    │   │
│  │  └───────────────────┘  └───────────────────┘  │   - 换行符统一            │    │   │
│  │                                                │ • ChunkAssembler          │    │   │
│  │                                                │   - TCP 分包重组          │    │   │
│  │                                                │   - 消息边界检测          │    │   │
│  │                                                │ • EncodingFixer           │    │   │
│  │                                                │   - BOM/空字节移除        │    │   │
│  │                                                └───────────────────────────┘    │   │
│  └─────────────────────────────────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────────────────────────────────┐   │
│  │                             Adapters Layer                                       │   │
│  │  ┌─────────────────────────────────┐  ┌─────────────────────────────────────┐   │   │
│  │  │       OpenAIProvider            │  │       AnthropicProvider             │   │   │
│  │  │  ─────────────────────────────  │  │  ─────────────────────────────────  │   │   │
│  │  │  • /v1/chat/completions         │  │  • /v1/messages                     │   │   │
│  │  │  • /v1/completions              │  │  • event: message_start/delta/stop  │   │   │
│  │  │  • 多模态支持 (Vision)          │  │  • content_block_delta              │   │   │
│  │  │  • Function calling             │  │                                     │   │   │
│  │  │  • JSON mode                    │  │                                     │   │   │
│  │  └─────────────────────────────────┘  └─────────────────────────────────────┘   │   │
│  └─────────────────────────────────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────────────────────────────────┐   │
│  │                              Ports Layer                                         │   │
│  │  trait Provider: parse_request | format_response | fix_stream_chunk | can_handle │   │
│  └─────────────────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────────────────┘
```

## 数据流

```
┌──────────────────────────────────────────────────────────────────────────────────────┐
│                                  请求处理流程                                         │
│                                                                                      │
│  Client (IDE/App)                                                                    │
│       │                                                                              │
│       │ HTTPS (api.openai.com:443)                                                   │
│       ▼                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────────────┐    │
│  │                         hosts 文件劫持                                       │    │
│  │              api.openai.com → 127.0.0.1 / ::1                               │    │
│  └─────────────────────────────────────────────────────────────────────────────┘    │
│       │                                                                              │
│       ▼                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────────────┐    │
│  │                    NetworkManager (QSslServer :443)                          │    │
│  │  ┌───────────────────────────────────────────────────────────────────────┐  │    │
│  │  │ 1. SSL/TLS 握手 (自签名证书, CA 已安装到系统信任链)                    │  │    │
│  │  │ 2. HTTP 请求解析 (parseHttpRequest)                                    │  │    │
│  │  │ 3. 认证验证 (verifyAuth: Authorization / x-api-key)                    │  │    │
│  │  │ 4. 路由分发 (handleModelsRequest / handleChatCompletionsRequest)       │  │    │
│  │  └───────────────────────────────────────────────────────────────────────┘  │    │
│  └─────────────────────────────────────────────────────────────────────────────┘    │
│       │                                                                              │
│       │ transformRequestBody (模型名称替换 + API 密钥注入)                           │
│       ▼                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────────────┐    │
│  │                    QNetworkAccessManager (ConnectionPool)                    │    │
│  │                         → 真实 API 服务器                                    │    │
│  └─────────────────────────────────────────────────────────────────────────────┘    │
│       │                                                                              │
│       │ 响应 (流式 SSE / 非流式 JSON)                                                │
│       ▼                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────────────┐    │
│  │                         FluxFix 整流处理                                     │    │
│  │  ┌───────────────────────────────────────────────────────────────────────┐  │    │
│  │  │ 流式响应:                                                              │  │    │
│  │  │   SSEFixer → JsonFixer → ChunkAssembler → FluxFixSseHandler           │  │    │
│  │  │                                                                        │  │    │
│  │  │ 流模式转换:                                                            │  │    │
│  │  │   流式→非流式: StreamAggregator.add_bytes() → finalize()              │  │    │
│  │  │   非流式→流式: StreamSplitter.split() → SSE chunks                    │  │    │
│  │  └───────────────────────────────────────────────────────────────────────┘  │    │
│  └─────────────────────────────────────────────────────────────────────────────┘    │
│       │                                                                              │
│       │ 响应转发                                                                     │
│       ▼                                                                              │
│  Client (IDE/App)                                                                    │
└──────────────────────────────────────────────────────────────────────────────────────┘
```

## 技术栈

| 层 | 技术 | 说明 |
|---|---|---|
| UI | Qt6 Widgets | QWidget, QStackedWidget, QListWidget, QTextEdit |
| 网络 | Qt6 Network | QSslServer, QNetworkAccessManager, QSslSocket |
| 平台 (Windows) | Win32 API | DPAPI, CryptoAPI, UAC, ShellExecuteEx |
| 平台 (Linux) | POSIX + libsecret | pkexec, update-ca-certificates |
| 平台 (macOS) | Security.framework | Keychain, osascript |
| 加密 | OpenSSL | X.509 证书生成 (EVP_PKEY, X509) |
| 配置 | yaml-cpp | YAML 序列化/反序列化 |
| 整流 | Rust (FluxFix) | simd-json, bytes, serde, thiserror |
| FFI | cbindgen | Rust → C 头文件自动生成 |
| 构建 | CMake 3.16+ | Qt6 + OpenSSL + yaml-cpp + FluxFix |
| 构建 (Rust) | Cargo | cdylib 输出 |
