# 上位机/下位机同机运行手册（Ubuntu）

本文说明如何在同一台 Ubuntu 上分别启动上位机（GUI）和下位机（daemon）。

## 1. 启动前确认

1. 已按 [ubuntu-cmake-quickstart.md](./ubuntu-cmake-quickstart.md) 完成编译。
2. `config/xiaozhi.ini` 已配置 `server.url` 与音频设备。
3. `control_plane` 端口未被占用（默认 `127.0.0.1:19090`）。

## 2. 启动顺序

建议顺序：

1. 先启动下位机：

```bash
./build-cmake/xiaozhi_daemon --config config/xiaozhi.ini
```

2. 再启动上位机：

```bash
./build-cmake/xiaozhi_gui
```

3. 在 GUI 中进入 `Connection Settings` 页面，点击 `Test Connection`。

说明：
- GUI 默认控制面地址为 `ws://127.0.0.1:19090`。
- GUI 建连后会自动发送 `subscribe_events` 订阅状态事件。

## 3. Auto 模式链路（观察点）

当前核心 auto 模式行为：

1. 设备连云端并发送 `hello`（不带 `mcp`）。
2. 收到云端 `hello` 后开始 `listen start(mode=auto, session_id=...)`。
3. 连续上行 Opus 音频。
4. 收到 `stt` 后立即停上行（即使还未收到 `listen stop`）。
5. 收到 `tts/start` 后进入播放态。
6. 收到 `tts/stop` 且播放缓冲清空后自动进入下一轮监听。
7. 自动模式下静音超时仅记录日志，不触发停录动作。

## 4. 页面功能对照

### 4.1 Connection Settings

- 配置 `server endpoint` 与 API key。
- 触发连接测试命令。
- 显示连接状态与运行状态文本。

### 4.2 Audio & Hardware

- 调整输入增益、输出音量、降噪开关。
- 下发 `set_audio_config` 命令。

### 4.3 Live Conversation

- 展示会话状态。
- 展示 STT/LLM 文本。
- 支持连接/断开控制命令。

## 5. 联调命令建议

开两个终端：

终端 A（daemon）：

```bash
./build-cmake/xiaozhi_daemon --config config/xiaozhi.ini
```

终端 B（GUI）：

```bash
./build-cmake/xiaozhi_gui
```

若只测试下位机，可单独运行：

```bash
./build/xiaozhi_client --config config/xiaozhi.ini
```

## 6. 常见报错与处理

### 6.1 GUI 启动失败：`module "QtQuick.Controls" is not installed`

安装：

```bash
sudo apt install -y qml6-module-qtquick-controls qml6-module-qtquick-layouts
```

### 6.2 daemon 正常，GUI 无法连接

排查：
- 检查 `control_plane.bind_host` 与 `control_plane.port`。
- 检查端口占用：`ss -lntp | rg 19090`。
- 查看 daemon 日志是否有控制面启动信息。

### 6.3 音频设备不可用

排查：
- `arecord -l`、`aplay -l` 确认设备名。
- 校验 `config/xiaozhi.ini` 的 `audio.capture_device`、`audio.playback_device`。

### 6.4 自动模式看起来“没有停录”

说明：
- 自动模式下静音超时是“记录但忽略动作”，属于设计行为。
- 真正停录触发点是收到服务器 `stt`。

## 7. 手工 GUI 三页联调验收

### 7.1 验收前准备

```bash
cmake -S . -B build-cmake
cmake --build build-cmake -j
```

### 7.2 启动服务

终端 A：

```bash
./build-cmake/xiaozhi_daemon --config config/xiaozhi.ini
```

终端 B：

```bash
./build-cmake/xiaozhi_gui
```

### 7.3 三页验收项

Connection Settings：
- 可看到默认控制面地址 `ws://127.0.0.1:19090`。
- 点击 `Test Connection` 后状态从 `connecting` 进入 `connected` 或可见错误提示。
- daemon 侧有控制面连接日志。

Audio & Hardware：
- 调整滑块/开关不崩溃，界面状态可实时变化。
- 触发 `set_audio_config` 后收到 `command_ack`（当前为占位 no-op）。

Live Conversation：
- 可看到 `state_changed` 事件驱动的状态变化。
- 点击 `Connect/Disconnect` 后状态变化与 daemon 日志一致。
- STT/LLM 文本区域可接收并展示事件文本。

## 8. 分支合并/提交流程

以下示例以 `feature/upper-lower-gui` 合并到 `feature/code_check` 为例：

```bash
git checkout feature/code_check
git pull --ff-only
git merge --no-ff feature/upper-lower-gui
```

合并后建议再次执行：

```bash
cmake -S . -B build-cmake
cmake --build build-cmake -j
ctest --test-dir build-cmake --output-on-failure
```

若需要推送远端：

```bash
git push origin feature/code_check
```
