# Xiaozhi OTA Activation Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add Scheme A startup behavior so the client performs OTA bootstrap first, exits with an activation code when manual activation is required, and otherwise uses OTA-returned websocket settings.

**Architecture:** Add a small OTA module that builds the request, runs `curl`, and parses the response. Feed the parsed result into `app_init()` so websocket initialization only happens after OTA bootstrap succeeds or a safe fallback is chosen.

**Tech Stack:** C11, cJSON, existing config loader, existing app runtime, external `curl` binary

---

## Chunk 1: Config and OTA parsing

### Task 1: Extend config parsing for OTA/bootstrap metadata

**Files:**
- Modify: `src/config.h`
- Modify: `src/config.c`
- Modify: `config/xiaozhi.ini.example`
- Test: `tests/test_config.c`

- [ ] **Step 1: Write the failing tests**

Add assertions in `tests/test_config.c` for `ota.url`, `board.type`, `board.name`, and default `ota.accept_language`.

- [ ] **Step 2: Run test to verify it fails**

Run: `make test TEST=test_config`
Expected: FAIL because the new config fields do not exist yet.

- [ ] **Step 3: Write minimal implementation**

Add OTA and board config structs, parse their INI sections, and validate required fields for Scheme A startup.

- [ ] **Step 4: Run test to verify it passes**

Run: `make test TEST=test_config`
Expected: PASS.

### Task 2: Add OTA response parsing

**Files:**
- Create: `src/ota_client.h`
- Create: `src/ota_client.c`
- Test: `tests/test_ota_client.c`
- Modify: `Makefile`

- [ ] **Step 1: Write the failing tests**

Add tests for:
- activation-required OTA response
- activated OTA response with websocket settings
- malformed response

- [ ] **Step 2: Run test to verify it fails**

Run: `make test TEST=test_ota_client`
Expected: FAIL because the OTA parser does not exist.

- [ ] **Step 3: Write minimal implementation**

Implement response structs and parsing helpers in `ota_client.c`.

- [ ] **Step 4: Run test to verify it passes**

Run: `make test TEST=test_ota_client`
Expected: PASS.

## Chunk 2: Bootstrap runtime

### Task 3: Add OTA bootstrap execution before websocket init

**Files:**
- Modify: `src/app.h`
- Modify: `src/app.c`
- Modify: `src/main.c`
- Modify: `src/ota_client.h`
- Modify: `src/ota_client.c`
- Test: `tests/test_app_bootstrap.c`

- [ ] **Step 1: Write the failing tests**

Extend bootstrap tests to cover activation-pending startup.

- [ ] **Step 2: Run test to verify it fails**

Run: `make test TEST=test_app_bootstrap`
Expected: FAIL because activation-pending state is not represented yet.

- [ ] **Step 3: Write minimal implementation**

Add OTA bootstrap in `app_init()`, store activation status in runtime, skip websocket/audio module initialization when activation is pending, and report the activation code clearly.

- [ ] **Step 4: Run targeted tests**

Run:
- `make test TEST=test_app_bootstrap`
- `make test TEST=test_config`
- `make test TEST=test_ota_client`

Expected: PASS.

### Task 4: Wire OTA-based websocket settings and fallback logic

**Files:**
- Modify: `src/app.c`
- Modify: `src/ota_client.c`
- Test: `tests/test_ota_client.c`

- [ ] **Step 1: Write the failing test**

Add a test proving OTA websocket fields are preferred over static server config when present.

- [ ] **Step 2: Run test to verify it fails**

Run: `make test TEST=test_ota_client`
Expected: FAIL because websocket settings are not surfaced from OTA response yet.

- [ ] **Step 3: Write minimal implementation**

Expose parsed websocket settings from OTA and use them when building `xiaozhi_client_config_t`.

- [ ] **Step 4: Run focused verification**

Run:
- `make test TEST=test_ota_client`
- `make test TEST=test_app_bootstrap`

Expected: PASS.

## Chunk 3: Final verification

### Task 5: Verify the end-to-end startup behavior

**Files:**
- Modify: `src/app.c`
- Modify: `src/ota_client.c`

- [ ] **Step 1: Build the app**

Run: `make`
Expected: build succeeds.

- [ ] **Step 2: Run focused regression tests**

Run:
- `make test TEST=test_config`
- `make test TEST=test_ota_client`
- `make test TEST=test_app_bootstrap`

Expected: all pass.

- [ ] **Step 3: Sanity-check runtime messaging**

Inspect logs in code to ensure activation-required startup prints the activation code and restart guidance without attempting websocket startup.
