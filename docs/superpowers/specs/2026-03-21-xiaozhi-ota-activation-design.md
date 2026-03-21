# Xiaozhi OTA Activation Design

## Goal

Adjust the client startup flow so it does not connect to WebSocket immediately. On startup, the client must first call the OTA endpoint, print any returned activation code for manual activation, and only use OTA-returned WebSocket settings after the device has been activated and the client is restarted.

## Current Behavior

The current runtime loads `server.url` and `server.token` from `config/xiaozhi.ini`, initializes the websocket client during `app_init()`, and immediately attempts to connect when `app_run()` starts. That skips the OTA bootstrap step required by the device API.

## Scheme A

Scheme A is intentionally minimal:

1. Load local identity and board metadata from config.
2. Build an OTA HTTP POST request that follows the documented headers and JSON body.
3. Execute the OTA request before websocket initialization.
4. If the OTA response contains `activation.code`, log the activation code and exit without opening the websocket.
5. If the OTA response does not require activation and includes websocket settings, initialize the websocket client from those OTA fields instead of static `server.url` / `server.token`.
6. Keep the existing audio and session flow unchanged once websocket startup begins.

This gives the user a safe manual activation checkpoint without redesigning the runtime around polling or background activation retries.

## Runtime Design

### Config

Add minimal OTA request configuration while preserving existing fields:

- `[ota]`
- `url`: OTA endpoint URL
- `accept_language`: optional language header
- `app_version`: current firmware/application version sent in `application.version`
- `elf_sha256`: optional firmware hash sent in `application.elf_sha256`

- `[board]`
- `type`: required board type
- `name`: required board name / SKU
- `ssid`: optional network name
- `rssi`: optional RSSI

Keep `[server]` as a fallback only. Scheme A should prefer OTA-returned websocket settings when available.

### OTA Client

Introduce a focused OTA bootstrap module with two responsibilities:

1. Build the request payload and shell-safe `curl` invocation using config values.
2. Parse the OTA JSON response into a small runtime struct:
   - `activation.code`
   - `activation.message`
   - `websocket.url`
   - `websocket.token`
   - `mqtt` and `firmware` fields may be parsed later, but they are not required for Scheme A startup.

Because `libcurl` development headers are not present in the current environment, Scheme A uses the installed `curl` binary as a pragmatic first step. The module boundary keeps a later move to native HTTP isolated.

### App Bootstrap

Update startup order:

1. `app_init()` loads config and initializes lightweight runtime state.
2. `app_init()` runs OTA bootstrap before websocket client creation.
3. If activation is required:
   - mark runtime as activation-pending
   - print activation code and message
   - skip websocket/audio module startup
4. If OTA succeeds with websocket settings:
   - store OTA websocket URL and token in runtime
   - initialize websocket/audio modules using OTA values

### User Experience

If activation is required, the client should emit clear logs such as:

- `activation required`
- `activation code: <code>`
- `please activate the device, then restart the client`

The process should exit cleanly with a non-success status so automation can tell that the device is not yet ready to chat.

## Error Handling

- OTA request failure: treat as startup failure and do not connect WebSocket.
- Invalid OTA JSON: treat as startup failure with a specific parse error.
- Missing websocket settings in a non-activation response: fall back to configured `[server]` values only if present.
- Missing required OTA config such as `ota.url`, `board.type`, or `board.name`: fail config validation.

## Testing

Add tests that cover:

- config parsing for new OTA and board fields
- OTA response parsing for:
  - activation-required response
  - activated response with websocket settings
  - malformed response
- bootstrap behavior that refuses websocket startup when activation is pending

## Non-Goals

- automatic polling for activation completion
- firmware download or update execution
- MQTT connection management
- background token refresh while the process is running
