# Xiaozhi Upper/Lower Control-Plane GUI Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a two-process Ubuntu desktop solution where `xiaozhi_daemon` (lower machine core + control plane) and `xiaozhi_gui` (upper machine Qt/QML) communicate via WebSocket JSON while preserving existing auto dialog behavior.

**Architecture:** Keep current C runtime behavior as the single source of truth, add a dedicated `control_plane` module inside the daemon for command/event bridging, and implement a separate Qt/QML process for the three UI pages. Migrate build from Makefile-only to CMake targets without breaking existing C test coverage.

**Tech Stack:** C11, C++17, Qt6 (Core/Quick/Qml/WebSockets), libwebsockets, ALSA, Opus, CTest.

---

**Spec reference:** `docs/superpowers/specs/2026-03-28-xiaozhi-upper-lower-control-plane-gui-design.md`

## File Structure Map

### Create
- `CMakeLists.txt` - project entry and target orchestration.
- `cmake/Dependencies.cmake` - pkg-config and Qt dependency wiring.
- `src/control_plane/control_protocol.h`
- `src/control_plane/control_protocol.c`
- `src/control_plane/control_plane_server.h`
- `src/control_plane/control_plane_server.c`
- `src/control_plane/control_events.h`
- `src/control_plane/control_events.c`
- `src/daemon/main_daemon.c` - lower-machine process entry.
- `src/daemon/daemon_runtime.h`
- `src/daemon/daemon_runtime.c` - adapter between `app_runtime_t` and control plane.
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

### Modify
- `src/app.h` - expose minimal runtime control/event hooks for daemon adapter.
- `src/app.c` - publish core state/protocol events to external observer callback.
- `src/config.h` - add control-plane config struct.
- `src/config.c` - parse and validate `[control_plane]` section defaults.
- `config/xiaozhi.ini.example` - add control-plane defaults and comments.
- `tests/test_config.c` - config parsing/default assertions for control-plane fields.

### Keep (No behavior rewrite)
- `src/session.c`
- `src/audio_capture.c`
- `src/audio_playback.c`
- `src/xiaozhi_protocol.c`

## Chunk 1: Build & Runtime Foundations

### Task 1: Introduce CMake skeleton and keep current core buildable

**Files:**
- Create: `CMakeLists.txt`
- Create: `cmake/Dependencies.cmake`
- Test: `tests_python/test_cmake_configure.py`

- [ ] **Step 1: Write the failing test**

```python
def test_cmake_configure_generates_cache(tmp_path):
    import subprocess
    build_dir = tmp_path / "build-cmake"
    rc = subprocess.run(["cmake", "-S", ".", "-B", str(build_dir)], capture_output=True, text=True)
    assert rc.returncode == 0, rc.stderr
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pytest -q tests_python/test_cmake_configure.py::test_cmake_configure_generates_cache`  
Expected: FAIL (`CMakeLists.txt` missing or configure error)

- [ ] **Step 3: Write minimal implementation**

```cmake
cmake_minimum_required(VERSION 3.20)
project(xiaozhi C CXX)
enable_testing()
add_subdirectory(gui EXCLUDE_FROM_ALL)
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pytest -q tests_python/test_cmake_configure.py::test_cmake_configure_generates_cache`  
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt cmake/Dependencies.cmake tests_python/test_cmake_configure.py gui/CMakeLists.txt
git commit -m "build: add initial CMake project skeleton"
```

### Task 2: Add `xiaozhi_core` and `xiaozhi_daemon` CMake targets

**Files:**
- Modify: `CMakeLists.txt`
- Create: `src/daemon/main_daemon.c`
- Test: `tests_python/test_cmake_targets.py`

- [ ] **Step 1: Write the failing test**

```python
def test_cmake_has_daemon_target(tmp_path):
    import subprocess
    b = tmp_path / "b"
    subprocess.check_call(["cmake", "-S", ".", "-B", str(b)])
    out = subprocess.check_output(["cmake", "--build", str(b), "--target", "help"], text=True)
    assert "xiaozhi_daemon" in out
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pytest -q tests_python/test_cmake_targets.py::test_cmake_has_daemon_target`  
Expected: FAIL (`xiaozhi_daemon` target not found)

- [ ] **Step 3: Write minimal implementation**

```cmake
add_library(xiaozhi_core STATIC ${CORE_SRCS})
add_executable(xiaozhi_daemon src/daemon/main_daemon.c)
target_link_libraries(xiaozhi_daemon PRIVATE xiaozhi_core)
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pytest -q tests_python/test_cmake_targets.py::test_cmake_has_daemon_target`  
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/daemon/main_daemon.c tests_python/test_cmake_targets.py
git commit -m "build: add xiaozhi_core and xiaozhi_daemon targets"
```

### Task 3: Add control-plane config fields with tests

**Files:**
- Modify: `src/config.h`
- Modify: `src/config.c`
- Modify: `tests/test_config.c`
- Modify: `config/xiaozhi.ini.example`

- [ ] **Step 1: Write the failing test**

```c
assert(strcmp(cfg.control_plane.bind_host, "127.0.0.1") == 0);
assert(cfg.control_plane.port == 19090);
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build-cmake --target test_config && ./build-cmake/tests/test_config`  
Expected: FAIL (missing `control_plane` fields)

- [ ] **Step 3: Write minimal implementation**

```c
typedef struct {
    char bind_host[64];
    int port;
} control_plane_config_t;
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build-cmake --target test_config && ctest --test-dir build-cmake -R test_config -V`  
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/config.h src/config.c tests/test_config.c config/xiaozhi.ini.example
git commit -m "config: add control plane host and port settings"
```

## Chunk 2: Dedicated Lower-Machine Control Plane

### Task 4: Implement command/event JSON codec

**Files:**
- Create: `src/control_plane/control_protocol.h`
- Create: `src/control_plane/control_protocol.c`
- Test: `tests/test_control_protocol.c`

- [ ] **Step 1: Write the failing test**

```c
assert(control_protocol_parse_command(json, &cmd) == 0);
assert(strcmp(cmd.name, "connect_server") == 0);
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build-cmake --target test_control_protocol && ./build-cmake/tests/test_control_protocol`  
Expected: FAIL (symbols not found)

- [ ] **Step 3: Write minimal implementation**

```c
int control_protocol_parse_command(const char *json, control_command_t *out);
int control_protocol_build_event(const control_event_t *event, char *buf, size_t n);
```

- [ ] **Step 4: Run test to verify it passes**

Run: `ctest --test-dir build-cmake -R test_control_protocol -V`  
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/control_plane/control_protocol.* tests/test_control_protocol.c
git commit -m "control-plane: add json command and event codec"
```

### Task 5: Map core runtime states/events to control-plane events

**Files:**
- Create: `src/control_plane/control_events.h`
- Create: `src/control_plane/control_events.c`
- Test: `tests/test_control_events.c`

- [ ] **Step 1: Write the failing test**

```c
assert(strcmp(control_events_state_name(SESSION_STATE_PLAYING_TTS), "playing") == 0);
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build-cmake --target test_control_events && ./build-cmake/tests/test_control_events`  
Expected: FAIL (missing mapping helpers)

- [ ] **Step 3: Write minimal implementation**

```c
const char *control_events_state_name(session_state_t state);
int control_events_from_protocol(const xiaozhi_incoming_event_t *in, control_event_t *out);
```

- [ ] **Step 4: Run test to verify it passes**

Run: `ctest --test-dir build-cmake -R test_control_events -V`  
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/control_plane/control_events.* tests/test_control_events.c
git commit -m "control-plane: add core-to-ui event mapping"
```

### Task 6: Build daemon runtime adapter and websocket control server

**Files:**
- Create: `src/daemon/daemon_runtime.h`
- Create: `src/daemon/daemon_runtime.c`
- Create: `src/control_plane/control_plane_server.h`
- Create: `src/control_plane/control_plane_server.c`
- Modify: `src/app.h`
- Modify: `src/app.c`
- Test: `tests/test_daemon_runtime.c`

- [ ] **Step 1: Write the failing test**

```c
assert(daemon_runtime_submit_command(&rt, &cmd) == 0);
assert(rt.last_command == DAEMON_CMD_CONNECT_SERVER);
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build-cmake --target test_daemon_runtime && ./build-cmake/tests/test_daemon_runtime`  
Expected: FAIL (adapter/server not implemented)

- [ ] **Step 3: Write minimal implementation**

```c
typedef void (*app_observer_fn)(const app_observer_event_t *ev, void *ctx);
int app_set_observer(app_runtime_t *app, app_observer_fn fn, void *ctx);
```

- [ ] **Step 4: Run test to verify it passes**

Run: `ctest --test-dir build-cmake -R "test_daemon_runtime|test_app_bootstrap|test_session" -V`  
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/daemon/* src/control_plane/control_plane_server.* src/app.[ch] tests/test_daemon_runtime.c
git commit -m "daemon: add dedicated control plane server and runtime bridge"
```

## Chunk 3: Upper-Machine Qt/QML GUI

### Task 7: Add Qt client bridge and ViewModel with tests

**Files:**
- Create: `gui/cpp/ControlClient.h`
- Create: `gui/cpp/ControlClient.cpp`
- Create: `gui/cpp/ConversationViewModel.h`
- Create: `gui/cpp/ConversationViewModel.cpp`
- Create: `gui/cpp/main.cpp`
- Test: `gui/tests/test_view_model.cpp`
- Modify: `gui/CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

```cpp
QCOMPARE(vm.connectionState(), QString("idle"));
vm.applyEvent(R"({"type":"event","name":"state_changed","payload":{"state":"uploading"}})");
QCOMPARE(vm.connectionState(), QString("uploading"));
```

- [ ] **Step 2: Run test to verify it fails**

Run: `ctest --test-dir build-cmake -R test_view_model -V`  
Expected: FAIL (ViewModel/bridge missing)

- [ ] **Step 3: Write minimal implementation**

```cpp
class ConversationViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString connectionState READ connectionState NOTIFY connectionStateChanged)
};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build-cmake --target test_view_model && ctest --test-dir build-cmake -R test_view_model -V`  
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add gui/cpp gui/tests/test_view_model.cpp gui/CMakeLists.txt
git commit -m "gui: add control websocket client and viewmodel"
```

### Task 8: Implement three QML pages and command wiring

**Files:**
- Create: `gui/qml/Main.qml`
- Create: `gui/qml/pages/ConnectionSettings.qml`
- Create: `gui/qml/pages/AudioHardware.qml`
- Create: `gui/qml/pages/LiveConversation.qml`
- Modify: `gui/cpp/ConversationViewModel.*`

- [ ] **Step 1: Write a failing GUI smoke test**

```cpp
QQmlApplicationEngine engine;
engine.load(QUrl("qrc:/qml/Main.qml"));
QVERIFY(!engine.rootObjects().isEmpty());
```

- [ ] **Step 2: Run test to verify it fails**

Run: `ctest --test-dir build-cmake -R test_qml_smoke -V`  
Expected: FAIL (`Main.qml` missing or load error)

- [ ] **Step 3: Write minimal implementation**

```qml
StackView {
    initialItem: ConnectionSettings {}
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build-cmake --target xiaozhi_gui test_qml_smoke && ctest --test-dir build-cmake -R "test_qml_smoke|test_view_model" -V`  
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add gui/qml gui/cpp/ConversationViewModel.*
git commit -m "gui: implement connection/audio/live pages and command wiring"
```

## Chunk 4: End-to-End Validation & Docs

### Task 9: Add end-to-end local smoke and auto-loop verification

**Files:**
- Create: `tests_python/test_local_upper_lower_smoke.py`
- Modify: `gui/cpp/ControlClient.cpp`
- Modify: `src/control_plane/control_plane_server.c`

- [ ] **Step 1: Write the failing integration test**

```python
def test_upper_lower_local_smoke():
    # start daemon, connect GUI client, assert state/event exchange
    assert "state_changed" in events
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pytest -q tests_python/test_local_upper_lower_smoke.py::test_upper_lower_local_smoke`  
Expected: FAIL (missing stable e2e handshake path)

- [ ] **Step 3: Write minimal implementation**

```c
/* server pushes snapshot after GUI subscribe */
control_plane_push_state_snapshot(...);
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pytest -q tests_python/test_local_upper_lower_smoke.py::test_upper_lower_local_smoke`  
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add tests_python/test_local_upper_lower_smoke.py gui/cpp/ControlClient.cpp src/control_plane/control_plane_server.c
git commit -m "test: add local upper-lower smoke and initial state sync"
```

### Task 10: Deliver CMake beginner docs and runbook

**Files:**
- Create: `docs/build/ubuntu-cmake-quickstart.md`
- Create: `docs/build/upper-lower-runbook.md`

- [ ] **Step 1: Write docs test checklist (failing by absence)**

```text
Must include: dependencies, configure/build commands, run order, troubleshooting
```

- [ ] **Step 2: Run manual check to verify docs missing/incomplete**

Run: `test -f docs/build/ubuntu-cmake-quickstart.md && test -f docs/build/upper-lower-runbook.md`  
Expected: FAIL before writing

- [ ] **Step 3: Write minimal implementation**

```markdown
1. sudo apt install ...
2. cmake -S . -B build-cmake
3. cmake --build build-cmake -j
4. ./build-cmake/xiaozhi_daemon
5. ./build-cmake/xiaozhi_gui
```

- [ ] **Step 4: Verify docs completeness**

Run: `rg -n "依赖|cmake -S|启动顺序|常见报错" docs/build/ubuntu-cmake-quickstart.md docs/build/upper-lower-runbook.md`  
Expected: all required topics present

- [ ] **Step 5: Commit**

```bash
git add docs/build/ubuntu-cmake-quickstart.md docs/build/upper-lower-runbook.md
git commit -m "docs: add cmake quickstart and upper-lower runbook"
```

### Task 11: Final regression and release checkpoint

**Files:**
- Modify: `docs/superpowers/plans/2026-03-28-xiaozhi-upper-lower-control-plane-gui.md` (mark completed during execution only)

- [ ] **Step 1: Run C unit/integration tests**

Run: `ctest --test-dir build-cmake --output-on-failure`  
Expected: all PASS

- [ ] **Step 2: Run Python tests**

Run: `pytest -q tests_python`  
Expected: PASS

- [ ] **Step 3: Manual runtime smoke**

Run:
```bash
./build-cmake/xiaozhi_daemon --config config/xiaozhi.ini
./build-cmake/xiaozhi_gui
```
Expected: GUI 三页面可切换；连接与状态事件正常；Live 页面可见 STT/TTS 状态流

- [ ] **Step 4: Record evidence**

Run: `git log --oneline -n 12`  
Expected: commits align with each task and are reviewable

- [ ] **Step 5: Commit final polish**

```bash
git add -A
git commit -m "chore: finalize upper-lower gui integration and verification"
```

## Execution Notes

1. Apply DRY/YAGNI: avoid changing protocol/audio behavior unless tests require it.
2. Keep auto mode invariants from spec unchanged:
   - `hello` 不带 `mcp`
   - auto `listen/start` 必带 `session_id`
   - 收到 `stt` 立即停上行
   - 自动模式忽略静音超时动作
3. Use `@superpowers/test-driven-development` before each implementation step.
4. Use `@superpowers/verification-before-completion` before claiming completion.
