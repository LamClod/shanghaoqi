# 上号器 (ShangHaoQi)

本地 HTTPS 代理网关，通过 DNS 劫持拦截 AI API 请求，在不同 AI 服务商协议之间进行透明转换。客户端应用无需任何修改，即可通过一个协议（如 OpenAI）无缝调用任意受支持的 AI 后端。

## 技术栈

| 组件 | 技术 |
|------|------|
| 语言 | C++23 |
| UI 框架 | Qt 6 Widgets |
| 网络 | Qt 6 Network（QSslServer、QNetworkAccessManager） |
| 构建系统 | CMake 3.28+ |
| 包管理 | vcpkg |
| SSL/TLS | OpenSSL |
| 测试框架 | Qt Test |
| 目标平台 | Windows、macOS、Linux |

## 功能列表

| 编号 | 功能 | 说明 |
|:----:|------|------|
| 1 | 多协议 AI API 代理 | 在 OpenAI、Anthropic、Gemini、Vercel AI SDK、Jina、Codex、Claude Code、Antigravity 等格式之间自动转换 |
| 2 | 15 种出站服务商 | OpenAI、Anthropic、Gemini、ZAI、百炼、ModelScope、Codex、ClaudeCode、Antigravity、OpenRouter、xAI、DeepSeek、豆包、Moonshot，以及任意 OpenAI 兼容接口 |
| 3 | 9 种入站协议 | OpenAI Chat Completions、OpenAI Responses、Anthropic Messages、Gemini、Vercel AI SDK、Jina、Codex CLI、Claude Code CLI、Antigravity |
| 4 | HTTPS 透明拦截 | 通过 DNS 劫持 + 自签名 CA + 本地 SSL 代理实现，客户端无感知 |
| 5 | 完整流式传输 | 在不同 SSE 流式格式之间互相转换，上下游独立的流式模式控制（跟随客户端 / 强制开启 / 强制关闭） |
| 6 | 连接池 | 可配置大小的 QNetworkAccessManager 连接池，支持连接复用和溢出处理 |
| 7 | 中间件管道 | 认证、模型映射、流式模式控制、调试日志四级中间件链，正向处理请求、反向处理响应 |
| 8 | 自动重试策略 | 可配置最大重试次数（默认 3），对限流、超时、服务不可用等瞬态错误自动重试 |
| 9 | 跨平台 | Windows、macOS、Linux 三平台各自有独立的证书管理、hosts 文件管理和权限提升实现 |
| 10 | API Key 加密存储 | Windows 使用 DPAPI 加密（SHA-256 熵值 = 应用名 + 机器 ID）；其他平台 Base64 回退 |
| 11 | 自动权限提升 | 启动时检测管理员权限，不足时通过 UAC（Windows）/ sudo（Linux/macOS）请求提升 |
| 12 | 配置导入导出 | JSON 格式，导出时使用可移植的 Base64 编码（不依赖平台 DPAPI） |
| 13 | API 健康检查 | 支持单个配置和批量全部测试，展示 HTTP 状态码和成功/失败统计 |
| 14 | 自动拉取模型列表 | 从服务商 API 获取可用模型，自动尝试多种认证方式（Bearer、Anthropic、Gemini） |
| 15 | 自定义 HTTP 头 | 每个配置组可设置任意自定义请求头 |
| 16 | 备用 URL 候选 | 每个配置组支持配置多个备用 API 地址 |
| 17 | 运行时参数可配置 | 代理端口、请求超时、连接超时、SSL 严格模式、HTTP/2 均可在界面上调整 |
| 18 | GUI 图形界面 | 配置组管理、全局设置、实时日志查看器三大页面 |
| 19 | 自动化测试 | 12 项单元测试覆盖验证、流式处理、管道、路由、协议往返和平台组件 |

## 架构设计

项目采用**六边形架构（端口与适配器）**模式，核心业务逻辑与具体协议完全解耦。

### 数据流

```
客户端应用 (Cursor、Claude Code 等)
    │
    │ HTTPS 请求发往 api.openai.com（被 hosts 文件重定向到 127.0.0.1）
    ▼
ProxyServer（QSslServer 监听 443 端口）
    │
    │ RequestRouter 根据 URL 路径匹配入站协议
    ▼
Pipeline 管道
    │
    ├── 中间件链（正向）：Auth → ModelMapping → StreamMode → Debug
    ▼
InboundMultiRouter → 具体 InboundAdapter（如 OpenAIChatAdapter）
    │
    │ decodeRequest() → SemanticRequest（协议无关的统一表示）
    ▼
Processor（语义层处理器）
    │
    ├── Policy.preflight() — 验证
    ├── Policy.plan() — 执行计划与重试策略
    ▼
OutboundMultiRouter → 具体 OutboundAdapter（如 AnthropicOutbound）
    │
    │ buildRequest() → ProviderRequest（目标服务商格式）
    ▼
QtExecutor（HTTP 客户端）
    │
    │ execute() 或 connectStream() → 实际 HTTPS 调用
    ▼
服务商 API（如 api.anthropic.com）
    │
    │ ProviderResponse
    ▼
OutboundAdapter.parseResponse() → SemanticResponse
    │
    ├── 中间件链（反向）：Debug → StreamMode → ModelMapping → Auth
    ▼
InboundAdapter.encodeResponse() → 客户端格式 JSON
    │
    ▼
ProxyServer → 客户端
```

### 启动流程（Bootstrap）

| 步骤 | 操作 | 说明 |
|:----:|------|------|
| 1 | 检查管理员权限 | 修改 hosts 文件和安装证书需要管理员权限，不足时自动请求提升 |
| 2 | 生成 SSL 证书 | 使用 OpenSSL 生成 CA 证书 + 针对所有劫持域名的服务器证书（含 SAN） |
| 3 | 安装 CA 证书 | 将 CA 安装到系统信任存储（Windows: certutil / macOS: security / Linux: update-ca-certificates） |
| 4 | 修改 hosts 文件 | 将目标域名（如 `api.openai.com`）重定向到 `127.0.0.1`，带 `# ShangHaoQi` 注释标记 |
| 5 | 启动 HTTPS 代理 | 在配置端口（默认 443）启动 QSslServer 监听 |

## 实现细节

### 1. 语义层（协议无关数据模型）

语义层定义了一套统一的数据模型，所有 AI 服务商的协议格式都先转换到这套模型，再从这套模型转换到目标格式。这是整个系统的核心抽象，位于 `src/semantic/`。

#### 1.1 核心类型枚举（`src/semantic/types.h`）

| 枚举名 | 用途 | 可选值 |
|--------|------|--------|
| `TaskKind` | 任务类型 | `Conversation`（对话）、`Embedding`（嵌入）、`Ranking`（排序）、`ImageGeneration`（图像生成） |
| `SegmentKind` | 内容片段类型 | `Text`（文本）、`Media`（媒体）、`Structured`（结构化数据） |
| `StopCause` | 停止原因 | `Completed`（正常完成）、`Length`（达到长度限制）、`ToolCall`（工具调用）、`ContentFilter`（内容过滤）、`Cancelled`（已取消） |
| `ErrorKind` | 错误类型 | `InvalidInput`、`Unauthorized`、`Forbidden`、`RateLimited`、`Unavailable`、`Timeout`、`Internal` |
| `StreamMode` | 流式模式 | `FollowClient`（跟随客户端设置）、`ForceOn`（强制开启）、`ForceOff`（强制关闭） |
| `FrameType` | 流式帧类型 | `Delta`（增量内容）、`ActionDelta`（工具调用增量）、`Finished`（完成）、`Failed`（失败）、`UsageDelta`（用量更新） |

#### 1.2 统一请求（`SemanticRequest`，`src/semantic/request.h`）

| 字段 | 类型 | 说明 |
|------|------|------|
| `target` | `Target` | 目标模型规格（模型名、服务商标识等） |
| `messages` | `QList<InteractionItem>` | 对话历史，每条消息由多个 `Segment` 组成 |
| `constraints` | `Constraints` | 生成约束：temperature、max_tokens、topP、topK 等 |
| `tools` | `QList<ActionSpec>` | 函数调用/工具定义列表 |
| `metadata` | `QVariantMap` | 路由和认证信息（inbound.format、provider_adapter、auth_key 等） |
| `extensions` | `Extension` | 扩展键值对，用于传递不在标准字段中的额外信息 |

#### 1.3 统一响应（`SemanticResponse`，`src/semantic/response.h`）

| 字段 | 类型 | 说明 |
|------|------|------|
| `kind` | `TaskKind` | 任务类型 |
| `candidates` | `QList<Candidate>` | 候选结果列表，每个包含输出 segments、工具调用、停止原因 |
| `usage` | `Usage` | token 用量统计（promptTokens、completionTokens、totalTokens） |
| `modelUsed` | `QString` | 实际使用的模型名 |
| `extensions` | `Extension` | 扩展键值对 |

#### 1.4 流式帧（`StreamFrame`，`src/semantic/frame.h`）

| 字段 | 类型 | 说明 |
|------|------|------|
| `type` | `FrameType` | 帧类型（Delta / ActionDelta / Finished / Failed / UsageDelta） |
| `deltaSegments` | `QList<Segment>` | 增量内容片段 |
| `actionDelta` | `ActionDelta` | 工具调用增量信息 |
| `usageDelta` | `Usage` | 用量增量 |
| `candidateIndex` | `int` | 候选索引 |
| `isFinal` | `bool` | 是否为最终帧 |
| `failure` | `DomainFailure` | 失败信息（仅 Failed 帧） |

#### 1.5 错误模型（`DomainFailure`，`src/semantic/failure.h/.cpp`）

整个系统使用 C++23 的 `std::expected<T, DomainFailure>` 作为统一的 `Result<T>` 返回类型。

| 字段/方法 | 类型 | 说明 |
|-----------|------|------|
| `kind` | `ErrorKind` | 错误类别枚举 |
| `code` | `QString` | 服务商特定的错误码 |
| `message` | `QString` | 人类可读的错误描述 |
| `retryable` | `bool` | 是否可重试 |
| `temporary` | `bool` | 是否为临时性错误 |
| `httpStatus()` | → `int` | 将 ErrorKind 映射为 HTTP 状态码（如 RateLimited → 429） |
| `toJson()` | → `QJsonObject` | 将错误序列化为 JSON |

#### 1.6 端口接口定义（`src/semantic/ports.h`）

| 接口 | 方法 | 说明 |
|------|------|------|
| `IInboundAdapter` | `decodeRequest(body, metadata)` | 将客户端原始请求体解码为 `SemanticRequest` |
| | `encodeResponse(SemanticResponse)` | 将统一响应编码为客户端格式的 JSON 字节 |
| | `encodeStreamFrame(StreamFrame)` | 将流式帧编码为客户端格式的 SSE 数据 |
| | `encodeFailure(DomainFailure)` | 将错误编码为客户端格式的错误 JSON |
| `IOutboundAdapter` | `buildRequest(SemanticRequest)` | 将统一请求构建为服务商格式的 HTTP 请求（`ProviderRequest`） |
| | `parseResponse(ProviderResponse)` | 将服务商的 HTTP 响应解析为 `SemanticResponse` |
| | `parseChunk(ProviderChunk)` | 将服务商的流式数据块解析为 `StreamFrame` |
| | `mapFailure(httpStatus, body)` | 将服务商的 HTTP 错误映射为 `DomainFailure` |
| `IExecutor` | `execute(ProviderRequest)` | 发送同步 HTTP 请求并返回 `ProviderResponse` |
| | `connectStream(ProviderRequest)` | 建立流式连接，返回可逐块读取的 `QNetworkReply` |
| `ICapabilityResolver` | `resolve(target)` | 查询目标模型的能力配置（支持的任务类型等） |

#### 1.7 请求验证（`src/semantic/validate.h/.cpp`）

| 验证规则 | 条件 | 失败时的 ErrorKind |
|----------|------|--------------------|
| 目标模型名非空 | `target.model` 不能为空字符串 | `InvalidInput` |
| 消息列表非空 | `messages` 列表长度 > 0 | `InvalidInput` |
| temperature 范围 | 0 ≤ temperature ≤ 2 | `InvalidInput` |
| topP 范围 | 0 ≤ topP ≤ 1 | `InvalidInput` |
| topK 范围 | topK ≥ 0 | `InvalidInput` |
| 工具名非空 | 如果定义了 tools，每个工具的 name 不能为空 | `InvalidInput` |

#### 1.8 执行策略（`src/semantic/policy.h/.cpp`）

| 方法 | 输入 | 输出 | 说明 |
|------|------|------|------|
| `preflight()` | `SemanticRequest` + `Capability` | `Result<void>` | 检查请求的任务类型是否被服务商能力配置所支持 |
| `plan()` | `SemanticRequest` + `ConfigGroup` | `ExecutionPlan` | 创建执行计划，包含目标模型名和最大重试次数 |
| `nextRetry()` | `DomainFailure` + 当前重试次数 | `bool` | 判断是否应重试：RateLimited / Unavailable / Timeout → 可重试，其他 → 不重试 |

### 2. 入站适配器（`src/adapters/inbound/`）

负责将客户端发来的各种格式的请求解码为 `SemanticRequest`，并将 `SemanticResponse` 编码回客户端格式。`InboundMultiRouter` 根据请求元数据中的 `inbound.format` 字段路由到对应的适配器。

| 适配器类 | 协议 ID | 对应 API 路径 | 委托关系 | 说明 |
|----------|---------|---------------|----------|------|
| `OpenAIChatAdapter` | `openai` | `/v1/chat/completions` | 无 | 解析 OpenAI Chat Completions 格式：`messages[]` 含 `role`/`content`，工具调用为 `tool_calls` 数组，响应为 `choices[]` |
| `OpenAIResponsesAdapter` | `openai.responses` | `/v1/responses` | 无 | 解析 OpenAI Responses API 格式 |
| `AnthropicAdapter` | `anthropic` | `/v1/messages` | 无 | 解析 Anthropic Messages 格式：`messages[]` 含 `content[]` 内容块数组，工具调用为 `tool_use`/`tool_result` 内容块类型 |
| `GeminiAdapter` | `gemini` | `/gemini/v1beta/models/*` | 无 | 解析 Gemini 格式：`contents[]` 含 `parts[]` 部件数组，工具调用为 `functionCall`/`functionResponse`，响应为 `candidates[]` |
| `AiSdkAdapter` | `aisdk` | — | 无 | 解析 Vercel AI SDK 格式 |
| `JinaAdapter` | `jina` | — | 委托给 OpenAI | Jina AI 格式，内部复用 OpenAI 适配器的编解码逻辑 |
| `CodexAdapter` | `codex` | — | 委托给 OpenAI/Responses | OpenAI Codex CLI 格式，内部复用 OpenAI/Responses 适配器 |
| `ClaudeCodeAdapter` | `claudecode` | — | 委托给 Anthropic | Claude Code CLI 格式，内部复用 Anthropic 适配器 |
| `AntigravityAdapter` | `antigravity` | — | 委托给 OpenAI/Responses | Antigravity AI 格式，内部复用 OpenAI/Responses 适配器 |

### 3. 出站适配器（`src/adapters/outbound/`）

负责将 `SemanticRequest` 构建为服务商格式的 HTTP 请求，并将服务商的响应解析回 `SemanticResponse`。`OutboundMultiRouter` 根据请求元数据中的 `provider_adapter` 字段路由到对应的适配器。

| 适配器类 | 适配器 ID | 继承关系 | 说明 |
|----------|-----------|----------|------|
| `OpenAIOutbound` | `openai` | 独立实现 | OpenAI 官方 API 格式 |
| `OpenAICompatOutbound` | （可变） | 作为基类 | 通用 OpenAI 兼容格式，是多个服务商适配器的基类 |
| `AnthropicOutbound` | `anthropic` | 独立实现 | Anthropic 官方 API 格式，处理 `content[]` 块和 `tool_use`/`tool_result` |
| `GeminiOutbound` | `gemini` | 独立实现 | Google Gemini 官方 API 格式，处理 `contents[]`/`parts[]` 和 `functionCall` |
| `ZaiOutbound` | `zai` | 继承 OpenAICompat | ZAI 服务商 |
| `BailianOutbound` | `bailian` | 继承 OpenAICompat | 阿里百炼（通义千问系列） |
| `ModelScopeOutbound` | `modelscope` | 继承 OpenAICompat | 阿里 ModelScope |
| `CodexOutbound` | `codex` | 继承 OpenAI | Codex，扩展自 OpenAI 出站 |
| `ClaudeCodeOutbound` | `claudecode` | 委托给 Anthropic | Claude Code，内部复用 Anthropic 出站 |
| `AntigravityOutbound` | `antigravity` | 继承 OpenAICompat | Antigravity AI |
| OpenAICompat 预置实例 | `openrouter` | OpenAICompat 实例 | OpenRouter 服务商 |
| OpenAICompat 预置实例 | `xai` | OpenAICompat 实例 | xAI 服务商 |
| OpenAICompat 预置实例 | `deepseek` | OpenAICompat 实例 | DeepSeek 服务商 |
| OpenAICompat 预置实例 | `doubao` | OpenAICompat 实例 | 豆包服务商 |
| OpenAICompat 预置实例 | `moonshot` | OpenAICompat 实例 | Moonshot 服务商 |

### 4. 多协议转换原理

新增一种协议只需实现一个适配器，而不需要为每对协议组合分别编写转换逻辑（N 个适配器 vs N^2 个转换器）。

| 协议 | 消息结构 | 工具调用格式 | 响应结构 | 转换到语义层的关键映射 |
|------|----------|------------|----------|----------------------|
| OpenAI | `messages[]` 含 `role` / `content` | `tool_calls[]` 数组，每个含 `function.name` + `function.arguments` | `choices[]` 含 `message` + `finish_reason` | `role` → InteractionItem.role；`content` → Segment(Text)；`tool_calls` → ActionSpec |
| Anthropic | `messages[]` 含 `content[]` 内容块数组 | `tool_use` 内容块（含 `name` + `input`）/ `tool_result` 内容块 | 顶层 `content[]` + `stop_reason` | `content[].type=text` → Segment(Text)；`tool_use` → ActionSpec；`tool_result` → ToolResult |
| Gemini | `contents[]` 含 `parts[]` 部件数组 | `functionCall`（含 `name` + `args`）/ `functionResponse` | `candidates[]` 含 `content.parts` + `finishReason` | `parts[].text` → Segment(Text)；`functionCall` → ActionSpec；`functionResponse` → ToolResult |

### 5. 处理器与流式传输

#### 5.1 处理器（`src/semantic/processor.h/.cpp`）

核心执行引擎，编排整个请求的处理流程。

| 步骤 | 操作 | 失败时的行为 |
|:----:|------|------------|
| 1 | 通过 `ICapabilityResolver` 解析目标模型的能力配置 | 返回错误 |
| 2 | 通过 `Policy.preflight()` 执行预检验证 | 返回 `DomainFailure` |
| 3 | 通过 `Policy.plan()` 创建 `ExecutionPlan`（含重试策略） | 返回错误 |
| 4 | `OutboundAdapter.buildRequest()` → `QtExecutor.execute()` → `OutboundAdapter.parseResponse()` | 进入重试判断 |
| 5 | 遇到瞬态失败时调用 `Policy.nextRetry()` 判断是否重试，重试则回到步骤 4 | 达到最大重试次数后返回最后一次错误 |

#### 5.2 流式会话（`src/semantic/stream_session.h/.cpp`）

| 步骤 | 操作 | 说明 |
|:----:|------|------|
| 1 | `executor->connectStream()` | 建立与服务商的流式 HTTP 连接 |
| 2 | 监听 `QNetworkReply::readyRead` | 有数据到达时触发回调 |
| 3 | 逐行读取 SSE 数据 | 解析 `data:` 前缀行，空行为事件分隔符 |
| 4 | `outbound->parseChunk()` | 将每个 SSE 事件解析为 `StreamFrame` |
| 5 | 发射 Qt 信号 | `frameReady(StreamFrame)` / `finished()` / `error(DomainFailure)` |

#### 5.3 流式辅助功能（`src/semantic/features/`）

| 组件 | 源文件 | 用途 | 工作原理 |
|------|--------|------|----------|
| StreamAggregator | `stream_aggregator.h/.cpp` | 将多个增量帧聚合为完整响应 | 累积文本 Segment、合并工具调用的 name/arguments 增量、跟踪 token 用量，最终输出一个完整 `SemanticResponse` |
| StreamSplitter | `stream_splitter.h/.cpp` | 将完整响应拆分为增量帧序列 | 将 `SemanticResponse` 的每个 Segment 和 ActionSpec 分别封装为 `StreamFrame`，末尾添加 Finished 帧 |

### 6. 管道与中间件（`src/pipeline/`）

#### 6.1 管道处理流程（`pipeline.h/.cpp`）

| 阶段 | 方向 | 操作 | 说明 |
|:----:|:----:|------|------|
| 1 | — | `InboundAdapter.decodeRequest(body)` | 将原始请求体解码为 `SemanticRequest` |
| 2 | 正向 | 中间件链依次处理请求 | Auth → ModelMapping → StreamMode → Debug |
| 3 | — | `Processor.process(request)` | 通过出站适配器和执行器与服务商交互 |
| 4 | 反向 | 中间件链逆序处理响应 | Debug → StreamMode → ModelMapping → Auth |
| 5 | — | `InboundAdapter.encodeResponse(response)` | 将统一响应编码回客户端格式 |

流式请求：`PipelineStreamSession` 包装 `StreamSession`，在编码前对每一帧应用中间件反向链。

#### 6.2 中间件详细（`src/pipeline/middlewares/`）

| 中间件 | 源文件 | 正向处理（请求） | 反向处理（响应） |
|--------|--------|-----------------|-----------------|
| `AuthMiddleware` | `auth_middleware.h/.cpp` | 从元数据取 `auth_key`，去除 `Bearer ` 前缀，与配置的密钥比对，不匹配返回 `Unauthorized` | 无操作 |
| `ModelMappingMiddleware` | `model_mapping_middleware.h/.cpp` | 将请求中的本地模型名替换为服务商模型名（记录原始名到元数据） | 将响应中的 `modelUsed` 替换回本地模型名 |
| `StreamModeMiddleware` | `stream_mode_middleware.h/.cpp` | 根据配置的 `upstreamStreamMode` 决定是否修改请求中的 stream 字段 | 根据 `downstreamStreamMode` 决定是否对响应进行聚合（ForceOff）或拆分（ForceOn） |
| `DebugMiddleware` | `debug_middleware.h/.cpp` | debugMode 启用时，通过 `LOG_DEBUG` 记录请求的完整 JSON | debugMode 启用时，记录响应/帧的完整 JSON |

### 7. 代理服务器（`src/proxy/`）

#### 7.1 ProxyServer（`proxy_server.h/.cpp`）

| 功能 | 实现方式 |
|------|----------|
| HTTPS 监听 | `QSslServer` 监听配置端口（默认 443），使用动态生成的 SSL 证书 |
| 请求解析 | 从 `QSslSocket` 逐字节读取原始 HTTP/1.1 请求，解析 method、path、headers、body |
| 路由分发 | `RequestRouter` 根据 URL 路径匹配入站协议，将请求交给 `Pipeline` 处理 |
| 同步响应 | 将 Pipeline 返回的 `SemanticResponse` 编码后直接写入 socket |
| 流式响应 | 使用 `SseWriter` 以 HTTP chunked transfer encoding + SSE 格式逐帧写入 socket |
| 模型列表 | 特殊处理 `/v1/models` GET 请求：从上游服务商获取原始模型列表，规范化后返回 |

#### 7.2 请求路由（`request_router.h/.cpp`）

| URL 路径 | HTTP 方法 | 匹配的入站协议 |
|----------|-----------|---------------|
| `/v1/chat/completions` | POST | `openai` |
| `/v1/messages` | POST | `anthropic` |
| `/v1/responses` | POST | `openai.responses` |
| `/gemini/v1beta/models/*` | POST | `gemini` |
| `/v1/models` | GET | `openai`（模型列表查询，非 AI 请求） |
| `/v1/embeddings` | POST | `openai` |
| `/v1/rerank` | POST | `openai` |
| `/v1/audio` | POST | `openai` |

#### 7.3 SSE Writer（`sse_writer.h/.cpp`）

| 阶段 | 写入内容 | 说明 |
|:----:|----------|------|
| 响应头 | `HTTP/1.1 200 OK` + `Transfer-Encoding: chunked` + `Content-Type: text/event-stream` | 建立 chunked + SSE 连接 |
| 每帧 | `data: {json}\n\n`（外层以 chunk 长度包裹） | 每个 StreamFrame 编码为一个 SSE 事件 |
| 结束 | `data: [DONE]\n\n` | 发送哨兵标记 |
| 终止 | `0\r\n\r\n` | 零长度 chunk 终止 HTTP chunked 传输 |

#### 7.4 连接池（`connection_pool.h/.cpp`）

| 属性 | 说明 |
|------|------|
| 实现 | 线程安全的 `QNetworkAccessManager` 实例池 |
| 大小 | 可配置，默认 10 |
| 获取策略 | 优先复用空闲连接，池空时创建新实例 |
| 溢出处理 | 超过最大池大小时创建临时实例，用完后销毁 |

### 8. DNS 劫持机制

| 步骤 | 操作 | 具体实现 |
|:----:|------|----------|
| 1 | 推导劫持域名 | Bootstrap 从所有配置组中推导域名：`canonicalHijackDomain()` 从 base URL 提取域名，`defaultHijackDomain()` 将适配器 ID 映射到已知域名（如 `openai` → `api.openai.com`），配置组可通过 `hijackDomainOverride` 手动覆盖 |
| 2 | 修改 hosts 文件 | 向系统 hosts 文件添加 `127.0.0.1 <domain> # ShangHaoQi` 条目 |
| 3 | 刷新 DNS 缓存 | Windows: `ipconfig /flushdns`；macOS: `dscacheutil -flushcache`；Linux: `systemd-resolve --flush-caches` |
| 4 | SSL 拦截 | 本地 HTTPS 代理在 443 端口使用 CA 签发的证书监听，证书 SAN 包含所有劫持域名及通配符子域名 |
| 5 | 关闭清理 | 自动移除所有带 `# ShangHaoQi` 注释的 hosts 条目，刷新 DNS |

### 9. 证书管理

#### 9.1 证书生命周期

| 阶段 | 操作 | 参数 |
|:----:|------|------|
| 生成 CA | 生成自签名 CA 证书 | RSA 2048 位，有效期 10 年 |
| 签发服务器证书 | CA 签发服务器证书 | SAN 包含所有劫持域名及 `*.<domain>` 通配符 |
| 安装 CA | 将 CA 安装到系统信任存储 | 使客户端信任代理的服务器证书 |
| 有效期检查 | 检查证书剩余有效期 | 30 天内过期时自动重新生成 |

#### 9.2 平台实现

| 平台 | 证书生成 | CA 安装命令 | 指纹/有效期读取 |
|------|----------|------------|----------------|
| Windows | OpenSSL CLI | `certutil -addstore Root <ca.crt>` | `QSslCertificate` |
| macOS | OpenSSL CLI | `security add-trusted-cert -d -r trustRoot -k /Library/Keychains/System.keychain <ca.crt>` | `QSslCertificate` |
| Linux | OpenSSL CLI | 复制到 `/usr/local/share/ca-certificates/` + `update-ca-certificates` | `QSslCertificate` |

### 10. 配置系统（`src/config/`）

#### 10.1 全局配置（`GlobalConfig`）

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `mappedModelId` | `QString` | 空 | 全局模型 ID 覆盖，非空时替换所有请求的模型名 |
| `authKey` | `QString` | 空 | 本地认证密钥，非空时所有请求必须携带匹配的 key |
| `hijackDomains` | `QStringList` | 自动推导 | DNS 劫持域名列表，从所有配置组自动推导 |

#### 10.2 配置组（`ConfigGroup`）

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `name` | `QString` | — | 配置组显示名称 |
| `provider` | `QString` | — | 入站适配器 ID（如 `openai`、`anthropic`、`gemini`） |
| `outboundAdapter` | `QString` | 空（自动推导） | 出站适配器 ID，留空时由系统根据 base URL 自动推导 |
| `baseUrl` | `QString` | — | 服务商 API 基础 URL（如 `https://api.openai.com`） |
| `baseUrlCandidates` | `QStringList` | 空 | 备用 URL 列表 |
| `modelId` | `QString` | — | 模型标识符（如 `gpt-4`、`claude-3-opus`） |
| `apiKey` | `QString` | — | API 密钥（磁盘上加密存储） |
| `middleRoute` | `QString` | `/v1` | URL 中间路径 |
| `maxRetryAttempts` | `int` | 3 | 最大重试次数 |
| `customHeaders` | `QMap<QString,QString>` | 空 | 自定义 HTTP 请求头 |
| `hijackDomainOverride` | `QString` | 空 | 手动覆盖劫持域名，非空时覆盖自动推导结果 |

#### 10.3 运行时选项（`RuntimeOptions`）

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `debugMode` | `bool` | `false` | 启用后 DebugMiddleware 记录请求/响应详细日志 |
| `proxyPort` | `int` | 443 | 代理监听端口 |
| `disableSslStrict` | `bool` | `false` | 跳过上游服务商证书验证 |
| `enableHttp2` | `bool` | `false` | 启用 HTTP/2 |
| `enableConnectionPool` | `bool` | `false` | 启用 QNetworkAccessManager 连接池 |
| `connectionPoolSize` | `int` | 10 | 连接池最大大小 |
| `requestTimeout` | `int` | 120000 | 请求超时（毫秒） |
| `connectionTimeout` | `int` | 30000 | 连接超时（毫秒） |
| `upstreamStreamMode` | `StreamMode` | `FollowClient` | 上游（到服务商）的流式模式 |
| `downstreamStreamMode` | `StreamMode` | `FollowClient` | 下游（到客户端）的流式模式 |

#### 10.4 配置持久化（`config_store.h/.cpp`）

| 特性 | 说明 |
|------|------|
| 存储路径 | `%AppData%/ShangHaoQi/shanghaoqi/config.json` |
| JSON 键名兼容 | 同时支持 snake_case 和 camelCase，读写双向兼容 |
| API Key 加密（Windows） | `CryptProtectData` / `CryptUnprotectData`（DPAPI），熵值 = SHA-256(应用名 + 机器 ID) |
| API Key 加密（其他平台） | Base64 编码 |
| 导入导出 | 始终使用可移植的 Base64 编码，不依赖平台 DPAPI |

#### 10.5 服务商路由（`provider_routing.h`）

| 函数 | 输入 | 输出 | 说明 |
|------|------|------|------|
| `detectModelListProvider()` | 适配器 ID 或 base URL | `ModelListProvider` 枚举 | 推断服务商类型：OpenAICompat / Anthropic / Gemini |
| `authModesForModelList()` | `ModelListProvider` | `QList<AuthMode>` | 返回拉取模型列表时应尝试的认证方式列表 |
| `canonicalHijackDomain()` | `ConfigGroup` | `QString` | 从配置组的 base URL 中提取域名 |
| `defaultHijackDomain()` | 适配器 ID | `QString` | 将适配器 ID 映射到已知域名（如 `openai` → `api.openai.com`） |
| `migrateProviderField()` | 旧版 provider 字符串 | 适配器 ID | 将旧版基于域名的 provider 迁移为适配器 ID |

### 11. 平台抽象层（`src/platform/`）

使用**抽象工厂模式**，通过 `PlatformFactory` 创建当前平台的实现。

#### 11.1 接口定义（`src/platform/interfaces/`）

| 接口 | 方法 | 说明 |
|------|------|------|
| `ICertManager` | `generateCA()` | 生成自签名 CA 证书 |
| | `generateServerCert(domains)` | 生成包含多域名 SAN 的服务器证书 |
| | `installCA(path)` | 将 CA 安装到系统信任存储 |
| | `uninstallCA()` | 从系统信任存储卸载 CA |
| | `fingerprint()` | 查询已安装 CA 的指纹 |
| | `expiresAt()` | 查询证书有效期 |
| `IHostsManager` | `addEntry(domain, ip)` | 向 hosts 文件添加条目 |
| | `removeEntry(domain)` | 从 hosts 文件移除条目 |
| | `hasEntry(domain)` | 查询是否已存在条目 |
| | `flushDns()` | 刷新系统 DNS 缓存 |
| `IPrivilegeManager` | `isAdmin()` | 检查当前进程是否拥有管理员权限 |
| | `requestElevation()` | 请求权限提升（UAC / sudo） |

#### 11.2 各平台实现对比

| 操作 | Windows | macOS | Linux |
|------|---------|-------|-------|
| 证书生成 | OpenSSL CLI | OpenSSL CLI | OpenSSL CLI |
| CA 安装 | `certutil -addstore Root` | `security add-trusted-cert` | 复制到 `/usr/local/share/ca-certificates/` + `update-ca-certificates` |
| hosts 文件路径 | `C:\Windows\System32\drivers\etc\hosts` | `/etc/hosts` | `/etc/hosts` |
| hosts 条目标记 | `# ShangHaoQi` 注释 | `# ShangHaoQi` 注释 | `# ShangHaoQi` 注释 |
| DNS 缓存刷新 | `ipconfig /flushdns` | `dscacheutil -flushcache` | `systemd-resolve --flush-caches` |
| 管理员检查 | `CheckTokenMembership` | `geteuid() == 0` | `geteuid() == 0` |
| 权限提升 | `ShellExecuteExW` + `runas` 动词（UAC） | `osascript` 或 `sudo` | `pkexec` 或 `sudo` |

### 12. 用户界面（`src/ui/`）

#### 12.1 主窗口布局（`main_widget.h/.cpp`）

| 区域 | 位置 | 宽度 | 内容 |
|------|------|------|------|
| 左侧边栏 | 左侧固定 | 160px | 应用标题"上号器"、导航按钮（配置组 / 全局设置 / 日志）、状态指示灯、启动/停止按钮 |
| 右侧内容区 | 右侧填充 | 剩余空间 | `QStackedWidget` 切换三个页面 |

窗口大小：900 x 600。关闭时如果服务正在运行，弹出确认对话框。

#### 12.2 页面导航

| 页面索引 | 页面名称 | 对应组件 | 功能 |
|:--------:|----------|----------|------|
| 0 | 配置组 | `ConfigGroupPanel` | 表格展示所有配置组，支持新增、编辑、删除、导入、导出、刷新、单项测试、全部测试 |
| 1 | 全局设置 | `GlobalSettingsPage` | 当前配置组选择器、本地认证密钥、`RuntimeOptionsPanel` |
| 2 | 日志 | `LogPanel` | 等宽字体（Consolas 9pt）实时日志查看器 |

#### 12.3 配置组面板列（`config_group_panel.h/.cpp`）

| 列 | 内容 | 说明 |
|----|------|------|
| 名称 | 配置组名称 | 用户自定义的显示名称 |
| 入站适配器 | 协议 ID | 如 `openai`、`anthropic` |
| 出站适配器 | 适配器 ID | 如 `openai`、`anthropic`，空则显示"自动" |
| 基础 URL | 服务商 URL | 如 `https://api.openai.com` |
| 模型 | 模型名 | 如 `gpt-4`、`claude-3-opus` |
| 中间路由 | URL 中间路径 | 如 `/v1` |
| API Key | 掩码显示 | 仅显示前后几位字符 |
| 状态 | 当前/非当前 | 标识是否为激活的配置组 |

#### 12.4 配置编辑对话框选项卡

| 选项卡 | 字段 | 说明 |
|--------|------|------|
| 基本 | 配置名称 | 自定义名称 |
| | 入站适配器 | 下拉选择（9 种） |
| | 出站适配器 | 下拉选择（15 种） |
| | 服务商 URL | base URL 输入框 |
| | 模型名 | 文本输入 + 从 API 自动拉取下拉 |
| | 中间路由 | URL 中间路径 |
| | API Key | 密码输入框 |
| 高级 | 最大重试次数 | 数字输入 |
| | 劫持域名覆盖 | 手动指定劫持域名 |
| | 自定义 HTTP 头 | 键值对表格（可增删） |
| | 备用 URL 候选 | URL 列表（可增删） |

#### 12.5 日志级别着色（`log_panel.h/.cpp`）

| 级别 | 数值 | 颜色 |
|:----:|:----:|------|
| DEBUG | 0 | 灰色 |
| INFO | 1 | 深色（默认） |
| WARNING | 2 | 橙色 |
| ERROR | 3 | 红色 |

#### 12.6 测试结果对话框（`test_result_dialog.h/.cpp`）

| 显示内容 | 说明 |
|----------|------|
| 配置组名称 | 被测试的配置组 |
| 状态图标 | 对号（成功）/ 叉号（失败） |
| HTTP 状态码 | 服务商返回的 HTTP 状态码 |
| 统计汇总 | 成功数 / 失败数 / 总数 |

### 13. 日志系统（`src/core/log_manager.h/.cpp`）

| 特性 | 说明 |
|------|------|
| 设计模式 | 单例模式 |
| 日志级别 | DEBUG(0)、INFO(1)、WARNING(2)、ERROR(3) |
| 日志宏 | `LOG_DEBUG(category, message)`、`LOG_INFO()`、`LOG_WARNING()`、`LOG_ERROR()` |
| UI 集成 | 通过 Qt 信号 `logEntry(int level, QString timestamp, QString category, QString message)` 推送到 LogPanel |

### 14. 自动化测试（`tests/`）

| 测试文件 | 测试目标 | 说明 |
|----------|----------|------|
| `tst_validate.cpp` | 请求验证 | 验证 SemanticRequest 的各项校验规则 |
| `tst_aggregator.cpp` | 流式聚合器 | 验证多个 StreamFrame 聚合为完整 SemanticResponse 的正确性 |
| `tst_splitter.cpp` | 流式拆分器 | 验证 SemanticResponse 拆分为 StreamFrame 序列的正确性 |
| `tst_policy.cpp` | 执行策略 | 验证 preflight、plan、nextRetry 的逻辑 |
| `tst_pipeline.cpp` | 管道 | 验证中间件链正向/反向处理和完整请求生命周期 |
| `tst_sse_parser.cpp` | SSE 解析 | 验证 SSE `data:` 行的解析逻辑 |
| `tst_routers.cpp` | 路由 | 验证 InboundMultiRouter 和 OutboundMultiRouter 的路由选择 |
| `tst_openai_roundtrip.cpp` | OpenAI 往返 | 验证 OpenAI 格式 → SemanticRequest → OpenAI 格式 的无损往返 |
| `tst_anthropic_roundtrip.cpp` | Anthropic 往返 | 验证 Anthropic 格式 → SemanticRequest → Anthropic 格式 的无损往返 |
| `tst_config_store.cpp` | 配置存储 | 验证 JSON 序列化/反序列化、API Key 加密/解密、键名兼容 |
| `tst_hosts_manager.cpp` | hosts 管理 | 验证 hosts 条目的增删查和标记识别 |
| `tst_cert_manager.cpp` | 证书管理 | 验证 CA 和服务器证书的生成、安装、有效期检查 |

## 项目结构

```
shanghaoqi/
├── CMakeLists.txt                          # 根构建文件
├── CMakePresets.json                       # 构建预设 (win-x64-debug)
├── vcpkg.json                              # 依赖声明 (Qt 6)
├── resources/                              # 资源文件
├── src/
│   ├── main.cpp                            # 应用入口
│   ├── core/                               # 启动编排 + 日志
│   ├── semantic/                           # 协议无关数据模型 + 处理器 + 流式会话
│   ├── adapters/inbound/                   # 9 个入站适配器（客户端侧）
│   ├── adapters/outbound/                  # 15 个出站适配器（服务商侧）
│   ├── adapters/executor/                  # HTTP 执行器
│   ├── adapters/capability/                # 静态能力解析器
│   ├── pipeline/                           # 管道 + 4 个中间件
│   ├── proxy/                              # HTTPS 代理 + 路由 + SSE + 连接池
│   ├── config/                             # 配置结构 + 持久化 + 服务商路由
│   ├── platform/                           # 平台抽象（证书/hosts/权限）
│   └── ui/                                 # Qt Widgets 界面
└── tests/                                  # 12 项单元测试
```

## 构建

### 前置条件

| 依赖 | 版本要求 |
|------|----------|
| CMake | 3.28+ |
| C++ 编译器 | 支持 C++23 |
| vcpkg | 最新版 |
| Qt 6 | Core、Widgets、Network、Concurrent、Test |
| OpenSSL | 系统安装或 vcpkg 提供 |

### 构建步骤

```bash
cmake --preset win-x64-debug
cmake --build build
```

### 运行测试

```bash
ctest --test-dir build
```
