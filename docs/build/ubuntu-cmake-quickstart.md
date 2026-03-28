# Ubuntu 下 CMake 快速上手（小智项目）

本文给第一次接触 CMake 的同学准备，按顺序执行即可。

## 1. 安装依赖

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake pkg-config git \
  libwebsockets-dev libopus-dev libasound2-dev \
  qt6-base-dev qt6-declarative-dev libqt6websockets6-dev
```

如需完整 QML 运行时，额外安装：

```bash
sudo apt install -y \
  qml6-module-qtquick qml6-module-qtquick-controls \
  qml6-module-qtquick-layouts qml6-module-qtquick-templates \
  qml6-module-qtquick-window \
  qml6-module-qtwebsockets \
  qml6-module-qtqml qml6-module-qtqml-workerscript
```

说明：
- 如果你暂时不编译 GUI，可不安装 Qt6 相关包。
- 未安装 Qt6 时，CMake 会自动跳过 `xiaozhi_gui` 目标。
- 可用以下命令验证 Qt6 是否安装成功：

```bash
pkg-config --modversion Qt6Core
pkg-config --modversion Qt6Quick
pkg-config --modversion Qt6WebSockets
```

## 2. 配置工程

```bash
cmake -S . -B build-cmake
```

成功后会看到 `build-cmake/` 目录生成。

## 3. 编译

```bash
cmake --build build-cmake -j
```

如果只想编译下位机：

```bash
cmake --build build-cmake --target xiaozhi_daemon -j
```

## 4. 运行测试

运行 CTest：

```bash
ctest --test-dir build-cmake --output-on-failure
```

运行 Python 测试：

```bash
pytest -q tests_python
```

## 5. 启动程序

下位机（daemon）：

```bash
./build-cmake/xiaozhi_daemon --config config/xiaozhi.ini
```

上位机（GUI，需 Qt6 可用时）：

```bash
./build-cmake/xiaozhi_gui
```

## 6. 常见问题排查

### 6.1 `Qt6 not found, skip gui targets`

原因：Qt6 开发包未安装。  
处理：安装 `qt6-base-dev qt6-declarative-dev libqt6websockets6-dev` 后重新 `cmake -S . -B build-cmake`。

### 6.2 `Could NOT find Opus` / `Could NOT find Libwebsockets`

原因：音频或网络依赖缺失。  
处理：安装 `libopus-dev libwebsockets-dev`，然后重新配置。

### 6.3 `No rule to make target xiaozhi_gui`

原因：Qt6 不可用，GUI target 被跳过。  
处理：先确认 Qt6 安装成功，再重新 configure。

### 6.4 运行后无声音/设备异常

处理建议：
- 检查 `config/xiaozhi.ini` 的音频设备名是否正确。
- 用 `arecord -l`、`aplay -l` 确认设备存在。
- 若是自动模式，静音超时日志会打印但不会主动停录（设计行为）。

### 6.5 `module "QtQuick.Templates" plugin "qtquicktemplates2plugin" not found`

处理：

```bash
sudo apt install -y qml6-module-qtquick-templates
```

### 6.6 `module "QtQuick.Window" is not installed`

处理：

```bash
sudo apt install -y qml6-module-qtquick-window
```
