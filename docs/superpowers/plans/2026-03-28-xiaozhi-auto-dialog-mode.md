# 小智 Auto 对话模式实现计划（中文）

> **给执行型智能体：** 需要使用 `superpowers:subagent-driven-development`（可用子代理时）或 `superpowers:executing-plans` 执行本计划。步骤使用 `- [ ]` 勾选格式跟踪。

**目标：** 增加可配置的 `runtime.dialog_mode=auto`，实现“收到服务端 `hello` 后自动开录、收到 `stt` 后停止上行、播放结束后自动进入下一轮监听”。

**架构思路：** 在不打散现有 `app/session/protocol` 主体结构的前提下，通过“模式开关 + 事件驱动”实现自动流程。`manual` 保持原有 Enter 触发行为，`auto` 由协议事件和播放完成事件驱动。

**技术栈：** C11、libwebsockets、ALSA、Opus、Makefile 单测体系。

---

## Auto 模式完整流程

### 1. 握手阶段

1. 设备建立 WebSocket 连接并发送 `hello`。  
2. 服务端返回 `hello`，携带 `session_id`。  
3. 设备保存 `session_id`，进入 `READY`。  

### 2. 自动监听与上行阶段

1. 设备在 `auto` 模式下，收到服务端 `hello` 后立即发送：

```json
{
  "session_id": "xxx",
  "type": "listen",
  "state": "start",
  "mode": "auto"
}
```

2. 设备开始持续上行 Opus 二进制音频帧。  
3. 收到服务端 `stt` 后，设备立即关闭上行并发送 `listen stop`。  
4. 静音超时事件在 `auto` 模式下忽略，不触发停录。  

### 3. 播放与续轮阶段

1. 收到 `tts/start` 后进入播放态。  
2. 服务端下发二进制音频帧，设备持续解码播放。  
3. 收到 `tts/stop` 且本地播放缓冲清空后，设备立刻回到 `READY` 并自动发送下一轮 `listen start`。  
4. 如无退出信号，流程持续循环。  

### 4. 本地输入控制

1. `auto` 模式下不再支持 Enter 开始/停止录音。  
2. 仅保留退出控制（`q` 或 `SIGINT`）。  

---

## 实施任务

### 任务 1：先写测试（Red）

**涉及文件：**
- 修改：`tests/test_xiaozhi_protocol.c`
- 修改：`tests/test_session.c`
- 修改：`tests/test_config.c`

- [ ] 步骤 1：补充 `listen start(auto)` 必带 `session_id` 的协议测试
- [ ] 步骤 2：补充上传态收到 `stt` 触发停录的状态机测试
- [ ] 步骤 3：补充 `runtime.dialog_mode` 默认值与解析测试
- [ ] 步骤 4：运行目标测试并确认先失败

### 任务 2：实现协议与配置（Green）

**涉及文件：**
- 修改：`src/xiaozhi_protocol.h`
- 修改：`src/xiaozhi_protocol.c`
- 修改：`src/config.h`
- 修改：`src/config.c`

- [ ] 步骤 1：将 auto 的 `listen start` 构造函数改为强制 `session_id`
- [ ] 步骤 2：新增并校验 `runtime.dialog_mode`
- [ ] 步骤 3：运行 `test_xiaozhi_protocol` 与 `test_config`

### 任务 3：实现自动运行时流程（Green）

**涉及文件：**
- 修改：`src/event_queue.h`
- 修改：`src/session.c`
- 修改：`src/app.c`

- [ ] 步骤 1：新增 `stt` 结果事件，驱动上行停止
- [ ] 步骤 2：在 auto 模式下收到服务端 `hello` 自动启动监听
- [ ] 步骤 3：在 auto 模式下收到 `stt` 自动停止上行
- [ ] 步骤 4：播放完成后自动开启下一轮监听
- [ ] 步骤 5：auto 模式仅保留退出，不再处理 Enter 开始/停止
- [ ] 步骤 6：auto 模式下静音超时保持忽略
- [ ] 步骤 7：运行 `test_session`

### 任务 4：回归验证

**涉及文件：**
- 仅验证

- [ ] 步骤 1：运行本次变更相关目标测试
- [ ] 步骤 2：运行更大范围测试并记录结果
