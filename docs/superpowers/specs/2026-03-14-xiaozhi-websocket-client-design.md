# 小智 WebSocket 客户端一阶段设计

## 概述

本设计将当前仓库从“本地 WebSocket 服务端骨架”调整为“Linux 桌面命令行语音终端”。目标是一阶段直接接入 `xiaozhi.me` 官方服务，使用 WebSocket 协议完成以下能力：

- 本地 Snowboy 离线唤醒
- 麦克风采集与 Opus 上行
- 小智协议握手、会话控制、文本事件处理
- TTS 二进制音频下行与扬声器播放
- 会话结束后自动回到待唤醒状态

一阶段保持命令行形态，不实现 UI、本地 HTTP 接口或本地 WebSocket 接口。

## 背景与约束

当前仓库已有基于 `libwebsockets` 的 C 代码，但主线是服务端角色：

- [src/main.c](/home/tanxzh/tanxzh/code/c/web-socket/src/main.c)
- [src/ws_server.c](/home/tanxzh/tanxzh/code/c/web-socket/src/ws_server.c)
- [src/http_server.c](/home/tanxzh/tanxzh/code/c/web-socket/src/http_server.c)
- [src/command.c](/home/tanxzh/tanxzh/code/c/web-socket/src/command.c)

这些文件围绕“浏览器连接本地服务端”设计，不适合作为小智设备端协议实现的长期基础。因此本设计选择保留 C 技术栈与 `libwebsockets`，但将仓库核心职责切换为 WebSocket 客户端。

当前工作区还包含可复用的 Snowboy 资产：

- `lib/snowboy/snowboy/include/snowboy-detect.h`
- `lib/snowboy/snowboy/lib/ubuntu64/libsnowboy-detect.a`
- 根目录下未跟踪的 `hixiaozhi.pmdl`、`nihao.pmdl`

## 目标

一阶段必须满足：

1. 在 Linux 桌面上，以命令行程序方式运行。
2. 使用本地配置文件连接 `xiaozhi.me` 官方服务。
3. 建立小智 WebSocket 音频通道，并携带所需握手头。
4. 本地持续监听麦克风，使用 Snowboy 触发唤醒。
5. 唤醒后发送协议握手与监听控制消息，并上传 Opus 音频帧。
6. 接收服务端文本消息与 TTS 二进制音频并播放。
7. 连接失败、鉴权失败、音频设备失败时给出清晰日志并安全恢复或退出。

## 非目标

一阶段明确不做：

- 图形界面
- 本地给 UI 使用的 HTTP / WebSocket API
- MCP / IoT 能力实现
- 交叉平台支持
- MQTT、HTTPS 或其他非 WebSocket 通道
- 二进制协议版本 2/3

## 外部协议依据

实现以以下文档为设计依据：

- 小智协议说明：<https://github.com/78/xiaozhi-esp32/blob/main/docs/websocket.md>
- 小智项目说明：<https://github.com/78/xiaozhi-esp32/blob/main/README_zh.md>

从协议文档中，一阶段明确采用并固化以下约束：

- 连接时带请求头：`Authorization`、`Protocol-Version`、`Device-Id`、`Client-Id`
- `Protocol-Version` 与 `hello.version` 保持一致
- 一阶段使用二进制协议版本 `1`
- 上行音频为 `opus`、`16000 Hz`、单声道、`60 ms` 帧
- 下行既有 JSON 文本消息，也有 Opus 二进制音频帧
- 服务端可能下发不同播放采样率，协议文档特别提到下行可能为 `24000 Hz`

## 方案选择

采用单进程、模块化的 C 客户端，而不是继续在当前服务端骨架上叠功能。

选择原因：

- 保留当前仓库已选定的 C / `libwebsockets` 路线
- 避免旧服务端回调和新客户端状态机互相缠绕
- 让后续 UI 可以围绕清晰的本地核心继续扩展

## 总体架构

程序拆分为六个核心单元：

1. `config`
   - 读取本地配置文件
   - 校验必填项
   - 提供运行时只读配置对象

2. `audio_capture`
   - 打开麦克风
   - 连续产生 `16 kHz / mono / s16le` PCM
   - 在空闲时供唤醒词检测使用
   - 在会话时供 Opus 编码使用

3. `wakeword_snowboy`
   - 包装 Snowboy C++ 接口
   - 接收小块 PCM 并回报唤醒事件
   - 不直接操作网络与播放

4. `xiaozhi_client`
   - 维护 `libwebsockets` 客户端连接
   - 发送握手头、JSON 控制消息、Opus 二进制帧
   - 接收 JSON 文本与 TTS 音频帧

5. `session`
   - 负责状态机与事件协调
   - 决定何时连接、何时开始上传、何时停止上传、何时播放、何时回到待唤醒

6. `audio_playback`
   - 播放服务端返回的 TTS 音频
   - 维护播放缓冲与播放结束事件

辅助单元：

- `opus_codec`：封装上行编码和下行解码
- `ring_buffer` / `event_queue`：线程间传输音频块与事件
- `log`：统一日志
- `app`：负责启动顺序、资源释放、信号处理

## 线程与运行模型

采用“主协调线程 + 三类工作线程”模型：

1. 主线程
   - 负责配置加载、模块初始化、事件循环、状态机协调、优雅退出

2. 采集线程
   - 持续读取麦克风 PCM
   - 空闲态下投递给 `wakeword_snowboy`
   - 会话态下同时投递给 Opus 编码路径

3. 网络线程
   - 承载 `libwebsockets` 事件循环
   - 发送文本 JSON 与二进制音频帧
   - 将服务端文本和音频数据转为内部事件

4. 播放线程
   - 消费下行音频缓冲
   - 驱动 ALSA 输出
   - 播放完成后回报事件

线程之间只交换两类对象：

- 事件：状态切换、网络状态、唤醒命中、播放完成、错误
- 音频块：PCM 或 Opus 的固定大小缓冲

不跨线程共享复杂业务对象。

## 状态机

一阶段状态机定义为：

- `IDLE`
- `WAKE_DETECTING`
- `CONNECTING`
- `HANDSHAKING`
- `READY`
- `UPLOADING_AUDIO`
- `WAITING_TTS`
- `PLAYING_TTS`
- `ERROR_BACKOFF`

其中：

- `IDLE` 只用于初始化前与优雅退出后的静止态，不作为正常运行时的常驻状态
- 程序正常运行时的空闲态是 `WAKE_DETECTING`

状态流转约定：

1. 启动完成后进入 `WAKE_DETECTING`
2. Snowboy 命中后进入 `CONNECTING`
3. WebSocket 建立且收到服务端 `hello` 后进入 `READY`
4. 发出 `listen` 控制消息并开始上传音频后进入 `UPLOADING_AUDIO`
5. 本地停止上传后进入 `WAITING_TTS`
6. 收到 `tts/start` 且有下行音频后进入 `PLAYING_TTS`
7. 收到 `tts/stop` 且播放结束后回到 `WAKE_DETECTING`
8. 任意阶段遇到网络中断或不可恢复协议错误，进入 `ERROR_BACKOFF`，随后回到 `WAKE_DETECTING`

## 协议映射

### 连接与握手

建立连接时必须设置：

- `Authorization: Bearer <token>`
- `Protocol-Version: 1`
- `Device-Id: <stable-device-id>`
- `Client-Id: <stable-client-id>`

连接成功后发送 `hello`：

```json
{
  "type": "hello",
  "version": 1,
  "transport": "websocket",
  "audio_params": {
    "format": "opus",
    "sample_rate": 16000,
    "channels": 1,
    "frame_duration": 60
  }
}
```

一阶段不声明 `features.mcp`，也不实现 `type: "mcp"` 的业务处理。

收到服务端 `hello` 后：

- 记录 `session_id`
- 记录服务端返回的 `audio_params`
- 将连接标记为可用

一阶段采用“按轮次建立连接”的策略：

- 待唤醒阶段不保持常驻 WebSocket 连接
- 唤醒命中后建立连接并完成一轮会话
- 当 `tts/stop` 已收到且播放缓冲清空后，主动关闭当前连接并回到待唤醒
- 多轮连续对话与常驻连接复用不在一阶段范围内

### 上行消息

一阶段实现以下上行文本消息：

- `hello`
- `listen` with `state: "detect"`
- `listen` with `state: "start"` and `mode: "auto"`
- `listen` with `state: "stop"`
- `abort` with `reason: "wake_word_detected"` when barge-in happens

一阶段的上行音频使用二进制协议版本 `1`，即直接发送 Opus 数据，不带额外头部。

### 下行消息

一阶段必须解析：

- `hello`
- `stt`
- `llm`
- `tts`
- `system`
- `mcp`
- `iot`
- 未知 `type`

处理规则：

- `hello`：建立会话上下文
- `stt`：打印识别文本
- `llm`：打印文本与情绪字段，供后续 UI 使用
- `tts/start`：切换到播放阶段，停止上行录音
- `tts/stop`：标记服务端本轮音频下发结束
- `system`：记录警告并忽略
- `mcp` / `iot`：记录为未实现能力并忽略，不参与一阶段会话逻辑
- 未知 `type` 或非法 JSON：记录原始摘要并忽略

收到二进制帧时：

- 若当前处于 `PLAYING_TTS` 或 `WAITING_TTS`，按 Opus 解码后进入播放缓冲
- 若当前仍处于录音上行阶段，丢弃二进制下行帧以避免录放冲突，并记录调试日志

## 音频策略

### 采集

- 采集格式固定为 `16 kHz / mono / s16le`
- 采集线程常驻，避免每轮唤醒都重新打开麦克风
- 以小块 PCM 推给 Snowboy；进入会话后复用同一份 PCM 给 Opus 编码器

### 唤醒

- 使用仓库内 Snowboy 静态库
- 通过一个 `C++` 包装层暴露纯 C 接口给主程序
- 唤醒命中后，先通知 `session`，不直接写网络

### 说话结束

一阶段使用本地静音超时结束上行录音：

- 当已开始上传后，若连续静音超过配置阈值，则发送 `listen/stop`
- 若服务端先发来 `tts/start`，立即停止上行并进入播放
- 另设最大单轮录音时长作为兜底

### 播放

- 使用 ALSA 播放
- 播放侧优先按服务端返回的采样率工作
- 为兼容桌面设备，默认使用 ALSA `plug` 设备，允许系统侧做必要重采样
- 若服务端长期固定返回单一采样率，则 Opus 解码器按该采样率初始化

## 配置与持久化

一阶段使用本地配置文件，建议路径为 `config/xiaozhi.ini`。

示例结构：

```ini
[server]
url = wss://api.tenclass.net/xiaozhi/v1/
token = <token>
protocol_version = 1

[device]
device_id =
client_id =

[audio]
capture_device = plughw:0,0
playback_device = plughw:0,0
input_sample_rate = 16000
silence_timeout_ms = 1200
max_utterance_ms = 15000

[snowboy]
resource = lib/snowboy/snowboy/resources/common.res
model = ./hixiaozhi.pmdl
sensitivity = 0.5
audio_gain = 1.0

[runtime]
log_level = info
reconnect_backoff_ms = 3000
```

字段策略：

- `url`、`token`、`snowboy.model` 必填
- `device.device_id` 为空时，默认从主网卡 MAC 推导
- `device.client_id` 为空时，首次生成 UUID 并持久化到 `state/client_id`
- `protocol_version` 一阶段固定校验为 `1`

说明：

- 配置文件只保存裸 `token`
- `Authorization: Bearer <token>` 请求头在运行时由客户端拼装，避免重复填写 `Bearer`

## 仓库重组

建议的主线文件结构：

- `src/main.c`
- `src/app.c/.h`
- `src/config.c/.h`
- `src/session.c/.h`
- `src/xiaozhi_client.c/.h`
- `src/audio_capture.c/.h`
- `src/audio_playback.c/.h`
- `src/opus_codec.c/.h`
- `src/wakeword_snowboy.cc/.h`
- `src/ring_buffer.c/.h`
- `src/log.c/.h`

现有文件处理策略：

- 保留并改造：
  - `src/main.c`
  - `src/msg_queue.c`
  - `src/msg_queue.h`

- 退出一阶段主线编译：
  - `src/ws_server.c`
  - `src/ws_server.h`
  - `src/http_server.c`
  - `src/http_server.h`
  - `src/command.c`
  - `src/command.h`

这些旧文件可暂时保留在仓库中，但不应再承担一阶段功能。

## 构建约束

一阶段构建需支持 C 与 C++ 混编：

- C 源文件继续使用 `gcc`
- Snowboy 包装层使用 `g++`
- 链接时加入 `libstdc++`
- 链接依赖至少包含：
  - `libwebsockets`
  - `libopus`
  - `libasound`
  - `pthread`
  - `lib/snowboy/snowboy/lib/ubuntu64/libsnowboy-detect.a`

## 错误处理

错误分为四类：

1. 配置错误
   - 缺 token、模型不存在、音频设备名为空
   - 启动即失败，打印具体字段

2. 可恢复网络错误
   - DNS 失败、TLS / WebSocket 握手失败、等待 `hello` 超时、服务端断开
   - 进入 `ERROR_BACKOFF`，清空本轮会话后自动重连

3. 会话级协议错误
   - 缺 `type`、缺 `session_id`、非法 JSON、未知文本消息
   - 记录日志并忽略；若影响会话完整性，则结束当前会话并回待唤醒

4. 本地音频错误
   - 麦克风打开失败、采集 underrun、播放失败、Opus 编码或解码失败
   - 优先重建相关模块；无法恢复时退出程序并输出原因

## 中断与打断策略

一阶段支持本地 barge-in：

- 如果程序处于 `PLAYING_TTS` 时再次检测到唤醒词：
  - 立即清空播放缓冲
  - 向服务端发送 `abort`
  - 关闭当前播放会话
  - 重新开始一轮连接或监听流程

Ctrl+C 必须触发优雅退出：

- 停止采集线程
- 关闭 WebSocket
- 排空或释放播放缓冲
- 释放 ALSA、Opus、Snowboy、`libwebsockets` 资源

## 日志与可观测性

命令行程序至少输出以下事件：

- 配置加载完成
- 待唤醒
- 唤醒词命中
- 开始连接 / 连接成功 / 握手成功
- 开始上传音频 / 停止上传音频
- 收到 `stt` / `llm` / `tts` 事件
- 播放开始 / 播放结束
- 网络错误 / 鉴权错误 / 音频错误

日志首要目标是帮助命令行调试，不做复杂日志系统设计。

## 测试策略

分三层验证：

1. 单元级
   - 配置解析
   - 状态机转移
   - 事件队列
   - JSON 消息编解码

2. 组件级
   - Snowboy 命中测试
   - Opus 编解码往返
   - 播放缓冲与播放结束事件
   - `xiaozhi_client` 的文本 / 二进制分流

3. 端到端手工验证
   - 程序启动后进入待唤醒
   - 说出唤醒词后连通官方服务
   - 能上传 16 kHz 单声道 Opus 音频
   - 能接收并播放 TTS 音频
   - 一轮会话结束后回到待唤醒
   - 断网、错误 token、麦克风占用时行为可解释

## 成功判定

当以下条件同时成立时，一阶段设计视为完成：

1. 命令行程序在 Linux 桌面上可稳定启动。
2. Snowboy 可使用本地模型触发唤醒。
3. 客户端可与 `xiaozhi.me` 建立带鉴权的 WebSocket 连接。
4. 能按协议发送 `hello`、`listen` 和 Opus 二进制帧。
5. 能接收并播放服务端 TTS 音频。
6. 会话结束后自动恢复到待唤醒状态。
7. 一阶段不引入 UI、本地 API、MCP 或其他额外子系统。
