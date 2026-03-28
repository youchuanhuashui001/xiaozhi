# 小智上位机/下位机 GUI 集成设计（同机双进程，控制面分离）

## 1. 目标

在当前工程内引入图形界面能力，满足以下目标：

1. 保留现有 C 核心对话能力（会话状态机、云端协议、音频采集/播放、Opus 编解码）。
2. 新增 Qt Quick/QML 上位机界面，提供三页交互：
   - Connection Settings
   - Audio & Hardware
   - Live Conversation
3. 与核心进程解耦：上位机和下位机通过独立控制面通信，不把 Qt 代码耦合进核心业务。
4. 首版先在同一台 Ubuntu 上以两个进程运行，后续可平滑迁移到局域网跨设备部署。
5. 构建系统从 Makefile 迁移到 CMake，并补充新手可执行的编译/运行文档。

## 2. 关键约束与已确认决策

### 2.1 约束

1. 界面技术栈使用 Qt Quick/QML。
2. 不直接在核心流程里混入 GUI 逻辑。
3. 需要“专门和上位机通信”的下位机模块。
4. 文档必须为中文，并包含 auto 模式流程说明。

### 2.2 已确认决策

1. 上位机与下位机在同一台 Ubuntu 上先跑通（双进程）。
2. 上位机/下位机通信采用 WebSocket + JSON（TCP，默认 127.0.0.1）。
3. 下位机新增专用 `control_plane` 模块，独立承载控制面协议。
4. 下位机保留现有自动对话核心逻辑，不让 GUI 参与 listen/stt/tts 协议判定。

## 3. 现状概述

当前工程核心流程由 `app.c` 驱动：

1. `app_init()`：配置加载、OTA 引导、运行时模块初始化。
2. `app_run()`：事件循环（event queue）+ session 状态机驱动动作。
3. 后台线程：
   - WebSocket 客户端线程（云端通信）
   - 音频采集线程
   - 音频播放线程

自动模式关键行为（当前已存在）：

1. 收到服务器 `hello` 后触发 `listen start(mode:auto)`。
2. 上行录音期间，收到 `stt` 后立即停止上行（即使尚未收到 `listen stop`）。
3. 收到 `tts/start` 进入播放态。
4. 收到 `tts/stop` 且播放缓冲清空后，自动进入下一轮监听。
5. 自动模式下静音超时仅记录，不触发停止动作。

## 4. 总体架构

### 4.1 进程划分

1. 下位机进程：`xiaozhi_daemon`（C）
2. 上位机进程：`xiaozhi_gui`（C++/QML）

### 4.2 架构关系

1. `xiaozhi_daemon` 内部包含：
   - `xiaozhi_core`（现有核心能力沉淀）
   - `xiaozhi_control_plane`（新增控制面通信能力）
2. `xiaozhi_gui` 通过 WebSocket 连接 `xiaozhi_control_plane`，进行命令下发和状态订阅。

### 4.3 设计原则

1. 核心能力单一来源：协议/音频/会话只在下位机实现。
2. 控制面协议前后端清晰边界：命令与事件分离。
3. 先同机后跨机：通信层不依赖本地专有接口，避免二次重构。

## 5. 下位机详细设计

### 5.1 模块职责

### `xiaozhi_core`

负责：

1. 云端 WebSocket 协议（hello/listen/stt/tts/binary）。
2. 录音采集、Opus 编码、播放解码与队列。
3. 会话状态机与自动模式策略。
4. 配置加载与运行时状态维护。

### `xiaozhi_control_plane`

负责：

1. 启动本地 WebSocket 服务端（默认 `127.0.0.1:<port>`）。
2. 解析上位机命令并转发为核心控制动作。
3. 将核心状态事件标准化后推送给上位机。
4. 连接管理、协议校验、错误应答。

### 5.2 下位机内部数据通路

1. 命令下行：GUI `command` -> `control_plane` -> 核心命令队列。
2. 事件上行：核心状态变化 -> `control_plane` -> GUI `event` 推送。
3. 控制面异常（协议错误/非法参数）仅影响控制面，不应中断核心会话线程。

### 5.3 核心与控制面集成点

新增（或等价）接口能力：

1. 核心控制接口（示意）：
   - `connect_server`
   - `disconnect_server`
   - `update_server_config`
   - `update_audio_config`
   - `request_shutdown`
2. 核心状态回调（示意）：
   - `on_state_changed`
   - `on_ws_status`
   - `on_protocol_event(stt/llm/tts)`
   - `on_audio_level`
   - `on_error`

## 6. 上位机详细设计

### 6.1 组件

1. `ControlClient`（C++）：负责控制面 WebSocket 连接、收发 JSON。
2. `ViewModel`（C++ QObject）：将协议字段映射为 QML 可绑定属性。
3. QML 页面：
   - Connection Settings
   - Audio & Hardware
   - Live Conversation

### 6.2 页面与功能映射

### Connection Settings

1. 配置 `server.url`、`server.token`。
2. `Test Connection` 触发 `test_connection` 命令并显示结果。
3. 显示连接状态（connected/error/retrying）。

### Audio & Hardware

1. 展示/切换输入输出设备。
2. 输入增益、输出音量、降噪开关配置。
3. 通过 `set_audio_config` 下发并接收 `command_ack`。

### Live Conversation

1. 展示会话状态（active/listening/playing/error）。
2. 展示 STT/LLM 对话消息流。
3. 展示实时输入音量峰值（由 `audio_level` 事件驱动）。

## 7. 控制面协议（v1）

所有消息为 JSON 文本帧。

### 7.1 通用结构

```json
{
  "type": "command | event | command_ack | error",
  "name": "connect_server",
  "request_id": "uuid-optional",
  "payload": {}
}
```

### 7.2 上位机 -> 下位机命令

1. `connect_server`
2. `disconnect_server`
3. `set_server_config`
4. `set_audio_config`
5. `test_connection`
6. `shutdown_client`

说明：

1. 首版控制面不提供手动 `listen start/stop` 命令。
2. 自动模式由下位机核心持续驱动，GUI 仅发连接/配置/退出类命令。

命令处理结果使用 `command_ack` 返回：

```json
{
  "type": "command_ack",
  "name": "set_audio_config",
  "request_id": "same-as-request",
  "ok": true,
  "message": "applied"
}
```

### 7.3 下位机 -> 上位机事件

1. `state_changed`：`idle/connecting/handshaking/ready/uploading/playing/error_backoff`
2. `ws_status`：`connected/disconnected` + code/message
3. `session_hello`：session_id 与音频参数
4. `stt_result`
5. `llm_text`
6. `tts_state`：`start/stop`
7. `audio_level`：峰值/电平
8. `error`：模块、错误码、可读信息

## 8. Auto 模式流程（重点）

以下流程由下位机核心保持，GUI 仅订阅显示：

1. 下位机连接云端并发送 `hello`。
2. `hello` 按既有约束不携带 `mcp` 字段。
3. 收到服务器 `hello` 后记录 `session_id` 并触发 `listen start`（mode=auto，携带 `session_id`）。
4. 下位机开始持续上行 Opus 二进制帧。
5. 收到 `stt` 后，立即关闭上行录音并发送 `listen stop`。
6. 收到 `tts/start` 后进入播放态并推送 `tts_state:start` 给 GUI。
7. 收到 `tts/stop` 后标记 TTS 完成；当播放缓冲清空，推送播放完成状态。
8. 下位机自动回到监听态，开始下一轮 `listen start`。
9. 自动模式下静音超时仅记录日志，不触发主动停录动作。

## 9. 错误处理与恢复

1. GUI 断开控制面连接：下位机继续运行，允许 GUI 随时重连。
2. 云端连接失败：进入 `error_backoff` 并按现有 backoff 策略重连，向 GUI 推送错误事件。
3. 音频设备异常：推送 `error` 事件，允许上位机更新设备配置后重试。
4. 非法命令/参数：返回 `command_ack(ok=false)`，不影响核心线程。

## 10. 测试策略

### 10.1 单元测试

1. 控制面协议编解码。
2. 命令参数校验与错误映射。
3. 核心状态 -> 控制面事件映射。

### 10.2 集成测试（同机双进程）

1. 启动 `xiaozhi_daemon`，再启动 `xiaozhi_gui`。
2. 验证三页面操作链路：
   - Connection Settings 可连通并显示状态
   - Audio & Hardware 配置生效
   - Live Conversation 显示 STT/LLM/TTS 与状态迁移
3. 验证 auto 流程循环与异常恢复路径。

### 10.3 回归测试

1. 现有 C 单测保持可执行。
2. 新增控制面冒烟测试，避免回归影响核心对话行为。

## 11. CMake 迁移与文档交付

### 11.1 CMake 目标

1. `xiaozhi_core`（静态库）
2. `xiaozhi_control_plane`（静态库）
3. `xiaozhi_daemon`（下位机可执行）
4. `xiaozhi_gui`（上位机可执行）

### 11.2 文档交付

新增中文文档（面向不熟悉 CMake 的用户）：

1. 环境依赖安装（Ubuntu）
2. 一步步编译命令（配置、构建、运行）
3. 双进程启动顺序
4. 常见报错与排查

文档建议路径：

1. `docs/build/ubuntu-cmake-quickstart.md`
2. `docs/build/upper-lower-runbook.md`

## 12. 分期实施建议

1. Phase 1：CMake 基建 + `xiaozhi_daemon` 跑通（含 control_plane 协议骨架）
2. Phase 2：`xiaozhi_gui` 三页面静态与状态绑定
3. Phase 3：控制面命令完整闭环 + auto 流程联调
4. Phase 4：测试完善与文档收敛

## 13. 非目标（本阶段不做）

1. 唤醒词链路重构
2. 跨设备鉴权体系完善（仅预留字段）
3. 移动端适配
4. 固件升级流程改造
