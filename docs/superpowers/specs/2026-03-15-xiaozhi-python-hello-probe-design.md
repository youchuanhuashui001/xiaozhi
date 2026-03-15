# 小智 Hello Python 探针设计

## 概述

本设计新增一个临时 Python 探针，用于复现当前 C 客户端的 WebSocket 握手与 `hello` 发送行为，并直接观察 `xiaozhi` 服务端是否返回文本、二进制、关闭事件或超时。

该探针的目标不是替代现有 C 客户端，也不负责录音、Opus、会话控制或播放。它只服务于当前排查问题：确认“连接建立后发送 `hello`，服务端到底有没有返回内容”。

## 背景与约束

当前 worktree 中的 C 客户端已经具备以下行为：

- 连接地址读取自 [config/xiaozhi.ini](/home/tanxzh/tanxzh/code/c/web-socket/.worktrees/xiaozhi-websocket-client/config/xiaozhi.ini)
- 握手头包含：
  - `Authorization: Bearer <token>`
  - `Protocol-Version`
  - `Device-Id`
  - `Client-Id`
- `hello` JSON 由 [src/xiaozhi_protocol.c](/home/tanxzh/tanxzh/code/c/web-socket/.worktrees/xiaozhi-websocket-client/src/xiaozhi_protocol.c) 构造

当前日志表现为：

- WebSocket 已连接
- `hello` 已发送
- 随后服务端直接断开
- 没有看到任何入站文本或二进制日志

因此需要一个更透明、依赖更少的探针，把问题从 C 运行时中剥离出来。

## 目标

这个临时探针必须满足：

1. 直接复用 [config/xiaozhi.ini](/home/tanxzh/tanxzh/code/c/web-socket/.worktrees/xiaozhi-websocket-client/config/xiaozhi.ini)。
2. 严格复现当前 C 客户端的握手头和 `hello` JSON。
3. 明确打印发出的 headers 和 `hello` payload。
4. 明确打印服务端返回的文本、二进制、关闭信息、超时或异常。
5. 可以作为 `pyenv` 环境下的临时排查脚本独立运行。

## 非目标

这个临时探针明确不做：

- 录音采集
- Opus 编解码
- `listen/start`、`listen/stop`、`abort`
- TTS 播放
- 通用协议调试器
- 与 C 客户端共享运行时代码

## 方案选择

采用“单文件 Python 脚本 + 极小依赖说明”的方案。

不采用可覆盖多种 `hello` 变体的调试器，也不采用 shell / `websocat` 一次性命令。原因是当前目标非常明确：用最小成本复现 C 客户端当前行为，并尽快确认服务端是否对这组握手和 `hello` 有响应。

## 文件结构

新增文件：

- [tools/xiaozhi_ws_probe.py](/home/tanxzh/tanxzh/code/c/web-socket/.worktrees/xiaozhi-websocket-client/tools/xiaozhi_ws_probe.py)
- [tools/requirements-xiaozhi-probe.txt](/home/tanxzh/tanxzh/code/c/web-socket/.worktrees/xiaozhi-websocket-client/tools/requirements-xiaozhi-probe.txt)

不修改现有 C 构建入口，不把该探针并入 `Makefile`。

## 行为设计

脚本启动后按以下顺序工作：

1. 读取 `--config` 指定的 ini 文件，默认使用 `config/xiaozhi.ini`。
2. 校验最小必填字段：
   - `server.url`
   - `server.token`
   - `server.protocol_version`
   - `device.device_id`
   - `device.client_id`
   - `audio.input_sample_rate`
3. 构造与当前 C 客户端一致的 headers：
   - `Authorization: Bearer <token>`
   - `Protocol-Version: <protocol_version>`
   - `Device-Id: <device_id>`
   - `Client-Id: <client_id>`
4. 构造与当前 C 客户端一致的 `hello`：
   - `type = hello`
   - `version = protocol_version`
   - `transport = websocket`
   - `audio_params.format = opus`
   - `audio_params.sample_rate = input_sample_rate`
   - `audio_params.channels = 1`
   - `audio_params.frame_duration = 60`
5. 建立 WebSocket 连接并发送 `hello`。
6. 在短超时窗口内持续接收入站事件并打印。

## 输出设计

探针输出固定为以下几个部分：

1. `config summary`
   - 打印 `url`、`protocol_version`、`device_id`、`client_id`
   - 不完整打印 token
2. `request headers`
   - 打印实际发送的 headers
   - `Authorization` 只显示前缀或部分脱敏内容
3. `hello payload`
   - 打印最终发送的 JSON
4. `server events`
   - `recv text: ...`
   - `recv binary: <n bytes>`
   - `closed: code=<code> reason=<reason>`
   - `timeout after hello`
   - `error: <exception>`

## 错误处理

探针只处理最有诊断价值的错误：

- 配置缺失时，直接失败并指出缺少的字段
- WebSocket / TLS / DNS / 代理异常时，打印异常原文
- 服务端主动关闭时，打印关闭码和 reason
- 连接成功但在超时窗口内没有任何消息时，打印超时结论

探针不做自动重连，也不做业务层 JSON 解析或语义推断。

## 测试策略

实现采用最小测试闭环：

1. 先写一个小的 Python 单元测试，验证：
   - 从 ini 读取出的关键字段正确
   - 构造出的 headers 与 `hello` 与 C 客户端当前逻辑一致
2. 观察该测试先失败，再补脚本实现使其通过
3. 手动运行探针，验证实际网络行为

手动验证命令目标为：

```bash
python tools/xiaozhi_ws_probe.py --config config/xiaozhi.ini
```

## 完成标准

以下条件满足时，认为这个临时探针完成：

1. 能在本地 Python 环境中运行。
2. 能成功读取 [config/xiaozhi.ini](/home/tanxzh/tanxzh/code/c/web-socket/.worktrees/xiaozhi-websocket-client/config/xiaozhi.ini)。
3. 发送内容与当前 C 客户端严格一致。
4. 能明确告诉使用者以下结果之一：
   - 收到文本消息
   - 收到二进制消息
   - 服务端直接关闭
   - 超时无返回
