# Xiaozhi Python Hello Probe Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a temporary Python probe that reads `config/xiaozhi.ini`, mirrors the current C client's websocket handshake and `hello` payload, and prints any server response after sending `hello`.

**Architecture:** Keep the probe separate from the C build. Implement a small Python module in `tools/` that handles config loading, request construction, and websocket I/O, plus a focused unit test that locks the generated headers and `hello` payload to the current C behavior.

**Tech Stack:** Python 3, `configparser`, `json`, `argparse`, `asyncio`, `websockets`, `unittest`

---

## Chunk 1: Probe scaffolding and payload parity

### Task 1: Add a failing unit test for config parsing and request construction

**Files:**
- Create: `tests_python/test_xiaozhi_ws_probe.py`
- Create: `tools/__init__.py`
- Test: `tests_python/test_xiaozhi_ws_probe.py`

- [ ] **Step 1: Write the failing test**

```python
import tempfile
import textwrap
import unittest
from pathlib import Path

from tools.xiaozhi_ws_probe import build_hello_payload, build_request_headers, load_probe_config


class ProbeConfigTest(unittest.TestCase):
    def test_loads_ini_and_builds_same_headers_and_hello_as_c_client(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            config_path = Path(tmpdir) / "xiaozhi.ini"
            config_path.write_text(textwrap.dedent(
                """
                [server]
                url = wss://api.tenclass.net/xiaozhi/v1/
                token = test
                protocol_version = 1

                [device]
                device_id = desktop-test
                client_id = client-test

                [audio]
                input_sample_rate = 16000
                """
            ).strip() + "\n", encoding="utf-8")

            cfg = load_probe_config(config_path)
            headers = build_request_headers(cfg)
            hello = build_hello_payload(cfg)

        self.assertEqual(cfg["url"], "wss://api.tenclass.net/xiaozhi/v1/")
        self.assertEqual(headers["Authorization"], "Bearer test")
        self.assertEqual(headers["Protocol-Version"], "1")
        self.assertEqual(headers["Device-Id"], "desktop-test")
        self.assertEqual(headers["Client-Id"], "client-test")
        self.assertEqual(
            hello,
            {
                "type": "hello",
                "version": 1,
                "transport": "websocket",
                "audio_params": {
                    "format": "opus",
                    "sample_rate": 16000,
                    "channels": 1,
                    "frame_duration": 60,
                },
            },
        )
```

- [ ] **Step 2: Run test to verify it fails**

Run: `python3 -m unittest tests_python.test_xiaozhi_ws_probe -v`

Expected: FAIL with `ModuleNotFoundError` or missing function errors because the probe module does not exist yet.

- [ ] **Step 3: Write minimal implementation**

```python
from configparser import ConfigParser


def load_probe_config(path):
    parser = ConfigParser()
    parser.read(path, encoding="utf-8")
    return {
        "url": parser.get("server", "url"),
        "token": parser.get("server", "token"),
        "protocol_version": parser.getint("server", "protocol_version"),
        "device_id": parser.get("device", "device_id"),
        "client_id": parser.get("device", "client_id"),
        "input_sample_rate": parser.getint("audio", "input_sample_rate"),
    }


def build_request_headers(cfg):
    return {
        "Authorization": f"Bearer {cfg['token']}",
        "Protocol-Version": str(cfg["protocol_version"]),
        "Device-Id": cfg["device_id"],
        "Client-Id": cfg["client_id"],
    }


def build_hello_payload(cfg):
    return {
        "type": "hello",
        "version": cfg["protocol_version"],
        "transport": "websocket",
        "audio_params": {
            "format": "opus",
            "sample_rate": cfg["input_sample_rate"],
            "channels": 1,
            "frame_duration": 60,
        },
    }
```

- [ ] **Step 4: Run test to verify it passes**

Run: `python3 -m unittest tests_python.test_xiaozhi_ws_probe -v`

Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add tests_python/test_xiaozhi_ws_probe.py tools/__init__.py tools/xiaozhi_ws_probe.py
git commit -m "test: lock python hello probe payload parity"
```

### Task 2: Add required-field validation and CLI entrypoint

**Files:**
- Modify: `tools/xiaozhi_ws_probe.py`
- Test: `tests_python/test_xiaozhi_ws_probe.py`

- [ ] **Step 1: Write the failing test**

```python
class ProbeValidationTest(unittest.TestCase):
    def test_missing_server_token_raises_clear_error(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            config_path = Path(tmpdir) / "xiaozhi.ini"
            config_path.write_text(textwrap.dedent(
                """
                [server]
                url = wss://api.tenclass.net/xiaozhi/v1/
                protocol_version = 1

                [device]
                device_id = desktop-test
                client_id = client-test

                [audio]
                input_sample_rate = 16000
                """
            ).strip() + "\n", encoding="utf-8")

            with self.assertRaisesRegex(ValueError, "missing server.token"):
                load_probe_config(config_path)
```

- [ ] **Step 2: Run test to verify it fails**

Run: `python3 -m unittest tests_python.test_xiaozhi_ws_probe -v`

Expected: FAIL because `load_probe_config()` does not yet emit the explicit `missing server.token` validation error.

- [ ] **Step 3: Write minimal implementation**

```python
def require(parser, section, option):
    if not parser.has_option(section, option) or parser.get(section, option).strip() == "":
        raise ValueError(f"missing {section}.{option}")
    return parser.get(section, option).strip()
```

Implement CLI parsing with:

```python
parser = argparse.ArgumentParser()
parser.add_argument("--config", default="config/xiaozhi.ini")
parser.add_argument("--timeout", type=float, default=5.0)
```

- [ ] **Step 4: Run test to verify it passes**

Run: `python3 -m unittest tests_python.test_xiaozhi_ws_probe -v`

Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add tests_python/test_xiaozhi_ws_probe.py tools/xiaozhi_ws_probe.py
git commit -m "feat: validate python hello probe config"
```

## Chunk 2: Websocket probe runtime and docs

### Task 3: Add the websocket runtime and visible diagnostics

**Files:**
- Modify: `tools/xiaozhi_ws_probe.py`
- Create: `tools/requirements-xiaozhi-probe.txt`

- [ ] **Step 1: Write the failing test**

Write a focused unit test for output helpers only, so the runtime can stay thin:

```python
class ProbeFormatTest(unittest.TestCase):
    def test_mask_authorization_value(self):
        self.assertEqual(mask_authorization("Bearer test"), "Bearer test...")
        self.assertEqual(mask_authorization(""), "")
```

- [ ] **Step 2: Run test to verify it fails**

Run: `python3 -m unittest tests_python.test_xiaozhi_ws_probe -v`

Expected: FAIL because `mask_authorization()` does not exist yet.

- [ ] **Step 3: Write minimal implementation**

Implement:
- `mask_authorization()`
- `async def run_probe(config_path, timeout_seconds):`
- websocket connection using `websockets.connect()`
- sending the JSON stringified `hello`
- printing:
  - `config summary`
  - `request headers`
  - `hello payload`
  - `recv text: ...`
  - `recv binary: <n bytes>`
  - `closed: code=<code> reason=<reason>`
  - `timeout after hello`
  - `error: <exception>`

Add dependency file:

```text
websockets>=15,<16
```

- [ ] **Step 4: Run test to verify it passes**

Run: `python3 -m unittest tests_python.test_xiaozhi_ws_probe -v`

Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add tests_python/test_xiaozhi_ws_probe.py tools/xiaozhi_ws_probe.py tools/requirements-xiaozhi-probe.txt
git commit -m "feat: add python websocket hello probe"
```

### Task 4: Verify the probe end to end in a Python environment

**Files:**
- Modify: `tools/xiaozhi_ws_probe.py` only if verification exposes a real defect

- [ ] **Step 1: Create the local Python environment**

Run:

```bash
cd /home/tanxzh/tanxzh/code/c/web-socket/.worktrees/xiaozhi-websocket-client
python3 -m venv .venv-probe
. .venv-probe/bin/activate
pip install -r tools/requirements-xiaozhi-probe.txt
```

Expected: `websockets` installs successfully.

- [ ] **Step 2: Run the unit tests in the environment**

Run:

```bash
. .venv-probe/bin/activate
python -m unittest tests_python.test_xiaozhi_ws_probe -v
```

Expected: PASS

- [ ] **Step 3: Run the actual probe**

Run:

```bash
. .venv-probe/bin/activate
python tools/xiaozhi_ws_probe.py --config config/xiaozhi.ini --timeout 5
```

Expected: The script prints the mirrored headers, the mirrored `hello` payload, and then one of:
- `recv text: ...`
- `recv binary: <n bytes>`
- `closed: code=... reason=...`
- `timeout after hello`
- `error: ...`

- [ ] **Step 4: If needed, apply the smallest fix and rerun verification**

Run:

```bash
. .venv-probe/bin/activate
python -m unittest tests_python.test_xiaozhi_ws_probe -v
python tools/xiaozhi_ws_probe.py --config config/xiaozhi.ini --timeout 5
```

Expected: PASS for unit tests and a clear runtime result from the probe.

- [ ] **Step 5: Commit**

```bash
git add tests_python/test_xiaozhi_ws_probe.py tools/xiaozhi_ws_probe.py tools/requirements-xiaozhi-probe.txt
git commit -m "chore: verify python hello probe"
```
