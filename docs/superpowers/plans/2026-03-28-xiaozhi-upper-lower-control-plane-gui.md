# 小智上位机/下位机控制面 GUI 实施计划

> **给执行型智能体：** 必须使用 `superpowers:subagent-driven-development`（若可用子代理）或 `superpowers:executing-plans` 来执行本计划。步骤使用 `- [ ]` 勾选格式跟踪。

**目标：** 构建一个 Ubuntu 同机双进程方案：`xiaozhi_daemon`（下位机核心 + 控制面）与 `xiaozhi_gui`（上位机 Qt/QML）通过 WebSocket JSON 通信，并保持现有 auto 对话行为不变。

**架构：** 保持当前 C 运行时为唯一事实来源，在 daemon 内新增独立 `control_plane` 模块做命令/事件桥接，并新增独立 Qt/QML 进程承载三页面 UI。构建系统从 Makefile 扩展到 CMake targets，同时不破坏现有 C 测试覆盖。

**技术栈：** C11、C++17、Qt6（Core/Quick/Qml/WebSockets）、libwebsockets、ALSA、Opus、CTest。

---

**对应规格文档：** `docs/superpowers/specs/2026-03-28-xiaozhi-upper-lower-control-plane-gui-design.md`

## 文件结构映射

### 新建
- `CMakeLists.txt` - 项目入口与 target 编排。
- `cmake/Dependencies.cmake` - pkg-config 与 Qt 依赖解析。
- `src/control_plane/control_protocol.h`
- `src/control_plane/control_protocol.c`
- `src/control_plane/control_plane_server.h`
- `src/control_plane/control_plane_server.c`
- `src/control_plane/control_events.h`
- `src/control_plane/control_events.c`
- `src/daemon/main_daemon.c` - 下位机进程入口。
- `src/daemon/daemon_runtime.h`
- `src/daemon/daemon_runtime.c` - `app_runtime_t` 与控制面的适配层。
- `gui/CMakeLists.txt`
- `gui/cpp/main.cpp`
- `gui/cpp/ControlClient.h`
- `gui/cpp/ControlClient.cpp`
- `gui/cpp/ConversationViewModel.h`
- `gui/cpp/ConversationViewModel.cpp`
- `gui/qml/Main.qml`
- `gui/qml/pages/ConnectionSettings.qml`
- `gui/qml/pages/AudioHardware.qml`
- `gui/qml/pages/LiveConversation.qml`
- `tests/test_control_protocol.c`
- `tests/test_control_events.c`
- `tests/test_daemon_runtime.c`
- `docs/build/ubuntu-cmake-quickstart.md`
- `docs/build/upper-lower-runbook.md`

### 修改
- `src/app.h` - 暴露 daemon 适配层需要的最小运行时控制/事件钩子。
- `src/app.c` - 将核心状态/协议事件发布给外部观察者回调。
- `src/config.h` - 增加 control-plane 配置结构。
- `src/config.c` - 解析并校验 `[control_plane]` 配置默认值。
- `config/xiaozhi.ini.example` - 增加 control-plane 默认配置与注释。
- `tests/test_config.c` - 增加 control-plane 字段的默认值/解析断言。

### 保持不改（不重写行为）
- `src/session.c`
- `src/audio_capture.c`
- `src/audio_playback.c`
- `src/xiaozhi_protocol.c`

## 阶段 1：构建与运行时基础

### 任务 1：引入 CMake 骨架并保证核心可构建

**文件：**
- 新建：`CMakeLists.txt`
- 新建：`cmake/Dependencies.cmake`
- 测试：`tests_python/test_cmake_configure.py`

- [ ] **步骤 1：先写失败测试**

```python
def test_cmake_configure_generates_cache(tmp_path):
    import subprocess
    build_dir = tmp_path / "build-cmake"
    rc = subprocess.run(["cmake", "-S", ".", "-B", str(build_dir)], capture_output=True, text=True)
    assert rc.returncode == 0, rc.stderr
```

- [ ] **步骤 2：运行测试并确认先失败**

执行：`pytest -q tests_python/test_cmake_configure.py::test_cmake_configure_generates_cache`  
预期：FAIL（缺失 `CMakeLists.txt` 或 configure 失败）

- [ ] **步骤 3：编写最小实现**

```cmake
cmake_minimum_required(VERSION 3.20)
project(xiaozhi C CXX)
enable_testing()
add_subdirectory(gui EXCLUDE_FROM_ALL)
```

- [ ] **步骤 4：运行测试并确认通过**

执行：`pytest -q tests_python/test_cmake_configure.py::test_cmake_configure_generates_cache`  
预期：PASS

- [ ] **步骤 5：提交**

```bash
git add CMakeLists.txt cmake/Dependencies.cmake tests_python/test_cmake_configure.py gui/CMakeLists.txt
git commit -m "build: add initial CMake project skeleton"
```

### 任务 2：增加 `xiaozhi_core` 与 `xiaozhi_daemon` CMake targets

**文件：**
- 修改：`CMakeLists.txt`
- 新建：`src/daemon/main_daemon.c`
- 测试：`tests_python/test_cmake_targets.py`

- [ ] **步骤 1：先写失败测试**

```python
def test_cmake_has_daemon_target(tmp_path):
    import subprocess
    b = tmp_path / "b"
    subprocess.check_call(["cmake", "-S", ".", "-B", str(b)])
    out = subprocess.check_output(["cmake", "--build", str(b), "--target", "help"], text=True)
    assert "xiaozhi_daemon" in out
```

- [ ] **步骤 2：运行测试并确认先失败**

执行：`pytest -q tests_python/test_cmake_targets.py::test_cmake_has_daemon_target`  
预期：FAIL（找不到 `xiaozhi_daemon` target）

- [ ] **步骤 3：编写最小实现**

```cmake
add_library(xiaozhi_core STATIC ${CORE_SRCS})
add_executable(xiaozhi_daemon src/daemon/main_daemon.c)
target_link_libraries(xiaozhi_daemon PRIVATE xiaozhi_core)
```

- [ ] **步骤 4：运行测试并确认通过**

执行：`pytest -q tests_python/test_cmake_targets.py::test_cmake_has_daemon_target`  
预期：PASS

- [ ] **步骤 5：提交**

```bash
git add CMakeLists.txt src/daemon/main_daemon.c tests_python/test_cmake_targets.py
git commit -m "build: add xiaozhi_core and xiaozhi_daemon targets"
```

### 任务 3：增加 control-plane 配置字段并补测试

**文件：**
- 修改：`src/config.h`
- 修改：`src/config.c`
- 修改：`tests/test_config.c`
- 修改：`config/xiaozhi.ini.example`

- [ ] **步骤 1：先写失败测试**

```c
assert(strcmp(cfg.control_plane.bind_host, "127.0.0.1") == 0);
assert(cfg.control_plane.port == 19090);
```

- [ ] **步骤 2：运行测试并确认先失败**

执行：`cmake --build build-cmake --target test_config && ./build-cmake/tests/test_config`  
预期：FAIL（缺少 `control_plane` 字段）

- [ ] **步骤 3：编写最小实现**

```c
typedef struct {
    char bind_host[64];
    int port;
} control_plane_config_t;
```

- [ ] **步骤 4：运行测试并确认通过**

执行：`cmake --build build-cmake --target test_config && ctest --test-dir build-cmake -R test_config -V`  
预期：PASS

- [ ] **步骤 5：提交**

```bash
git add src/config.h src/config.c tests/test_config.c config/xiaozhi.ini.example
git commit -m "config: add control plane host and port settings"
```

## 阶段 2：下位机专用控制面

### 任务 4：实现命令/事件 JSON 编解码

**文件：**
- 新建：`src/control_plane/control_protocol.h`
- 新建：`src/control_plane/control_protocol.c`
- 测试：`tests/test_control_protocol.c`

- [ ] **步骤 1：先写失败测试**

```c
assert(control_protocol_parse_command(json, &cmd) == 0);
assert(strcmp(cmd.name, "connect_server") == 0);
```

- [ ] **步骤 2：运行测试并确认先失败**

执行：`cmake --build build-cmake --target test_control_protocol && ./build-cmake/tests/test_control_protocol`  
预期：FAIL（符号不存在）

- [ ] **步骤 3：编写最小实现**

```c
int control_protocol_parse_command(const char *json, control_command_t *out);
int control_protocol_build_event(const control_event_t *event, char *buf, size_t n);
```

- [ ] **步骤 4：运行测试并确认通过**

执行：`ctest --test-dir build-cmake -R test_control_protocol -V`  
预期：PASS

- [ ] **步骤 5：提交**

```bash
git add src/control_plane/control_protocol.* tests/test_control_protocol.c
git commit -m "control-plane: add json command and event codec"
```

### 任务 5：把核心运行时状态映射为控制面事件

**文件：**
- 新建：`src/control_plane/control_events.h`
- 新建：`src/control_plane/control_events.c`
- 测试：`tests/test_control_events.c`

- [ ] **步骤 1：先写失败测试**

```c
assert(strcmp(control_events_state_name(SESSION_STATE_PLAYING_TTS), "playing") == 0);
```

- [ ] **步骤 2：运行测试并确认先失败**

执行：`cmake --build build-cmake --target test_control_events && ./build-cmake/tests/test_control_events`  
预期：FAIL（缺少映射函数）

- [ ] **步骤 3：编写最小实现**

```c
const char *control_events_state_name(session_state_t state);
int control_events_from_protocol(const xiaozhi_incoming_event_t *in, control_event_t *out);
```

- [ ] **步骤 4：运行测试并确认通过**

执行：`ctest --test-dir build-cmake -R test_control_events -V`  
预期：PASS

- [ ] **步骤 5：提交**

```bash
git add src/control_plane/control_events.* tests/test_control_events.c
git commit -m "control-plane: add core-to-ui event mapping"
```

### 任务 6：构建 daemon 运行时适配层与 WebSocket 控制服务

**文件：**
- 新建：`src/daemon/daemon_runtime.h`
- 新建：`src/daemon/daemon_runtime.c`
- 新建：`src/control_plane/control_plane_server.h`
- 新建：`src/control_plane/control_plane_server.c`
- 修改：`src/app.h`
- 修改：`src/app.c`
- 测试：`tests/test_daemon_runtime.c`

- [ ] **步骤 1：先写失败测试**

```c
assert(daemon_runtime_submit_command(&rt, &cmd) == 0);
assert(rt.last_command == DAEMON_CMD_CONNECT_SERVER);
```

- [ ] **步骤 2：运行测试并确认先失败**

执行：`cmake --build build-cmake --target test_daemon_runtime && ./build-cmake/tests/test_daemon_runtime`  
预期：FAIL（适配层/服务端未实现）

- [ ] **步骤 3：编写最小实现**

```c
typedef void (*app_observer_fn)(const app_observer_event_t *ev, void *ctx);
int app_set_observer(app_runtime_t *app, app_observer_fn fn, void *ctx);
```

- [ ] **步骤 4：运行测试并确认通过**

执行：`ctest --test-dir build-cmake -R "test_daemon_runtime|test_app_bootstrap|test_session" -V`  
预期：PASS

- [ ] **步骤 5：提交**

```bash
git add src/daemon/* src/control_plane/control_plane_server.* src/app.[ch] tests/test_daemon_runtime.c
git commit -m "daemon: add dedicated control plane server and runtime bridge"
```

## 阶段 3：上位机 Qt/QML GUI

### 任务 7：增加 Qt 客户端桥接与 ViewModel（含测试）

**文件：**
- 新建：`gui/cpp/ControlClient.h`
- 新建：`gui/cpp/ControlClient.cpp`
- 新建：`gui/cpp/ConversationViewModel.h`
- 新建：`gui/cpp/ConversationViewModel.cpp`
- 新建：`gui/cpp/main.cpp`
- 测试：`gui/tests/test_view_model.cpp`
- 修改：`gui/CMakeLists.txt`

- [ ] **步骤 1：先写失败测试**

```cpp
QCOMPARE(vm.connectionState(), QString("idle"));
vm.applyEvent(R"({"type":"event","name":"state_changed","payload":{"state":"uploading"}})");
QCOMPARE(vm.connectionState(), QString("uploading"));
```

- [ ] **步骤 2：运行测试并确认先失败**

执行：`ctest --test-dir build-cmake -R test_view_model -V`  
预期：FAIL（ViewModel/bridge 缺失）

- [ ] **步骤 3：编写最小实现**

```cpp
class ConversationViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString connectionState READ connectionState NOTIFY connectionStateChanged)
};
```

- [ ] **步骤 4：运行测试并确认通过**

执行：`cmake --build build-cmake --target test_view_model && ctest --test-dir build-cmake -R test_view_model -V`  
预期：PASS

- [ ] **步骤 5：提交**

```bash
git add gui/cpp gui/tests/test_view_model.cpp gui/CMakeLists.txt
git commit -m "gui: add control websocket client and viewmodel"
```

### 任务 8：实现三页面 QML 与命令绑定

**文件：**
- 新建：`gui/qml/Main.qml`
- 新建：`gui/qml/pages/ConnectionSettings.qml`
- 新建：`gui/qml/pages/AudioHardware.qml`
- 新建：`gui/qml/pages/LiveConversation.qml`
- 修改：`gui/cpp/ConversationViewModel.*`

- [ ] **步骤 1：先写失败的 GUI 冒烟测试**

```cpp
QQmlApplicationEngine engine;
engine.load(QUrl("qrc:/qml/Main.qml"));
QVERIFY(!engine.rootObjects().isEmpty());
```

- [ ] **步骤 2：运行测试并确认先失败**

执行：`ctest --test-dir build-cmake -R test_qml_smoke -V`  
预期：FAIL（`Main.qml` 缺失或加载失败）

- [ ] **步骤 3：编写最小实现**

```qml
StackView {
    initialItem: ConnectionSettings {}
}
```

- [ ] **步骤 4：运行测试并确认通过**

执行：`cmake --build build-cmake --target xiaozhi_gui test_qml_smoke && ctest --test-dir build-cmake -R "test_qml_smoke|test_view_model" -V`  
预期：PASS

- [ ] **步骤 5：提交**

```bash
git add gui/qml gui/cpp/ConversationViewModel.*
git commit -m "gui: implement connection/audio/live pages and command wiring"
```

## 阶段 4：端到端验证与文档

### 任务 9：增加同机端到端冒烟测试与 auto 循环验证

**文件：**
- 新建：`tests_python/test_local_upper_lower_smoke.py`
- 修改：`gui/cpp/ControlClient.cpp`
- 修改：`src/control_plane/control_plane_server.c`

- [ ] **步骤 1：先写失败的集成测试**

```python
def test_upper_lower_local_smoke():
    # start daemon, connect GUI client, assert state/event exchange
    assert "state_changed" in events
```

- [ ] **步骤 2：运行测试并确认先失败**

执行：`pytest -q tests_python/test_local_upper_lower_smoke.py::test_upper_lower_local_smoke`  
预期：FAIL（缺少稳定 e2e 握手路径）

- [ ] **步骤 3：编写最小实现**

```c
/* server pushes snapshot after GUI subscribe */
control_plane_push_state_snapshot(...);
```

- [ ] **步骤 4：运行测试并确认通过**

执行：`pytest -q tests_python/test_local_upper_lower_smoke.py::test_upper_lower_local_smoke`  
预期：PASS

- [ ] **步骤 5：提交**

```bash
git add tests_python/test_local_upper_lower_smoke.py gui/cpp/ControlClient.cpp src/control_plane/control_plane_server.c
git commit -m "test: add local upper-lower smoke and initial state sync"
```

### 任务 10：交付 CMake 新手文档与运行手册

**文件：**
- 新建：`docs/build/ubuntu-cmake-quickstart.md`
- 新建：`docs/build/upper-lower-runbook.md`

- [ ] **步骤 1：先写文档验收清单（缺失即失败）**

```text
必须包含：依赖安装、配置/构建命令、启动顺序、常见问题排查
```

- [ ] **步骤 2：运行检查并确认文档当前缺失/不完整**

执行：`test -f docs/build/ubuntu-cmake-quickstart.md && test -f docs/build/upper-lower-runbook.md`  
预期：编写前 FAIL

- [ ] **步骤 3：编写最小实现**

```markdown
1. sudo apt install ...
2. cmake -S . -B build-cmake
3. cmake --build build-cmake -j
4. ./build-cmake/xiaozhi_daemon
5. ./build-cmake/xiaozhi_gui
```

- [ ] **步骤 4：验证文档完整性**

执行：`rg -n "依赖|cmake -S|启动顺序|常见报错" docs/build/ubuntu-cmake-quickstart.md docs/build/upper-lower-runbook.md`  
预期：关键主题全部命中

- [ ] **步骤 5：提交**

```bash
git add docs/build/ubuntu-cmake-quickstart.md docs/build/upper-lower-runbook.md
git commit -m "docs: add cmake quickstart and upper-lower runbook"
```

### 任务 11：最终回归与发布检查点

**文件：**
- 修改：`docs/superpowers/plans/2026-03-28-xiaozhi-upper-lower-control-plane-gui.md`（仅在执行阶段勾选完成项）

- [ ] **步骤 1：运行 C 单测/集成测试**

执行：`ctest --test-dir build-cmake --output-on-failure`  
预期：全部 PASS

- [ ] **步骤 2：运行 Python 测试**

执行：`pytest -q tests_python`  
预期：PASS

- [ ] **步骤 3：手工运行冒烟**

执行：
```bash
./build-cmake/xiaozhi_daemon --config config/xiaozhi.ini
./build-cmake/xiaozhi_gui
```
预期：GUI 三页面可切换；连接与状态事件正常；Live 页面可看到 STT/TTS 状态流

- [ ] **步骤 4：记录证据**

执行：`git log --oneline -n 12`  
预期：提交粒度与任务一一对应，便于审阅

- [ ] **步骤 5：最终整理提交**

```bash
git add -A
git commit -m "chore: finalize upper-lower gui integration and verification"
```

## 执行备注

1. 遵循 DRY/YAGNI：除非测试要求，不改协议/音频核心行为。
2. 保持 spec 里的 auto 模式不变量：
   - `hello` 不带 `mcp`
   - auto `listen/start` 必带 `session_id`
   - 收到 `stt` 立即停止上行
   - 自动模式忽略静音超时动作
3. 每个实现任务前使用 `@superpowers/test-driven-development`。
4. 对外宣称“完成/通过”前使用 `@superpowers/verification-before-completion`。
