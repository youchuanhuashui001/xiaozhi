# 小智 WebSocket 客户端 Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a Linux desktop C command-line client that uses Snowboy wake-word detection, streams microphone audio to `xiaozhi.me` over WebSocket with Opus, and plays back returned TTS audio.

**Architecture:** Replace the current local WebSocket server path with a single-process client runtime composed of small modules: config, logging, event/state coordination, protocol codec, WebSocket client, audio capture/playback, Opus codec, and a Snowboy adapter. Keep business flow centered on a session state machine so audio, wake-word, and network code stay isolated and testable.

**Tech Stack:** C11, C++11 (Snowboy wrapper only), libwebsockets, libopus, ALSA, pthreads, cJSON, Snowboy static library

---

## Preconditions

- Install build dependencies: `sudo apt-get install -y build-essential pkg-config libwebsockets-dev libopus-dev libasound2-dev`
- Ensure `lib/snowboy/snowboy/lib/ubuntu64/libsnowboy-detect.a` exists in the workspace
- Ensure at least one local wake-word model exists before manual verification, for example `./hixiaozhi.pmdl`

## File Structure

### New runtime files

- Create: `src/app.h`
  Runtime bootstrap API for startup, shutdown, and the main coordination loop.
- Create: `src/app.c`
  Wires config, logging, session, WebSocket client, wake-word, capture, playback, and signal-driven shutdown together.
- Create: `src/log.h`
  Small logging API and log level enum.
- Create: `src/log.c`
  Timestamped console logging implementation.
- Create: `src/config.h`
  Runtime config structs, validation helpers, and ID generation interfaces.
- Create: `src/config.c`
  INI parsing, defaulting, validation, `device_id` and `client_id` resolution.
- Create: `src/event_queue.h`
  Thread-safe queue for fixed-size internal events.
- Create: `src/event_queue.c`
  Event queue implementation used by capture, network, playback, and session.
- Create: `src/audio_buffer.h`
  Fixed-size audio block queue definitions.
- Create: `src/audio_buffer.c`
  Queue implementation for PCM / Opus blocks shared across worker threads.
- Create: `src/session.h`
  Session state enum, event enum, and the pure coordination API.
- Create: `src/session.c`
  State transition logic and side-effect requests for the rest of the app.
- Create: `src/xiaozhi_protocol.h`
  C structs and helpers for protocol headers, outgoing JSON, and incoming JSON events.
- Create: `src/xiaozhi_protocol.c`
  `hello` / `listen` / `abort` serialization and incoming `hello` / `stt` / `llm` / `tts` / `system` parsing.
- Create: `src/xiaozhi_client.h`
  WebSocket client API and transport callback contracts.
- Create: `src/xiaozhi_client.c`
  `libwebsockets` client context, header injection, message send queue, callback dispatch, and reconnect/backoff handling.
- Create: `src/opus_codec.h`
  Opus encoder / decoder interfaces.
- Create: `src/opus_codec.c`
  `16k` upstream encoding and downstream decode setup.
- Create: `src/audio_capture.h`
  Capture thread API and PCM callback contract.
- Create: `src/audio_capture.c`
  ALSA capture loop, silence tracking, and PCM fan-out to wake-word + upstream encoder.
- Create: `src/audio_playback.h`
  Playback thread API and enqueue interface.
- Create: `src/audio_playback.c`
  ALSA playback loop for decoded TTS PCM and playback-finished notifications.
- Create: `src/wakeword_snowboy.h`
  Pure C API for creating, feeding, and destroying the Snowboy detector.
- Create: `src/wakeword_snowboy.cc`
  C++ adapter around `snowboy::SnowboyDetect`.

### Existing files to modify

- Modify: `Makefile`
  Stop wildcard-building every `src/*.c`; explicitly build the new client target, test targets, and the single C++ wrapper.
- Modify: `src/main.c`
  Replace server bootstrap with CLI entrypoint that loads config and delegates to `app_run`.

### Existing files to keep but remove from the main build

- Keep out of build: `src/ws_server.c`
- Keep out of build: `src/ws_server.h`
- Keep out of build: `src/http_server.c`
- Keep out of build: `src/http_server.h`
- Keep out of build: `src/command.c`
- Keep out of build: `src/command.h`
- Keep out of build: `src/sensor_sim.c`
- Keep out of build: `src/sensor_sim.h`
- Keep available: `src/cJSON.c`
- Keep available: `src/cJSON.h`
- Keep available until migrated or deleted: `src/msg_queue.c`
- Keep available until migrated or deleted: `src/msg_queue.h`

### New config and test files

- Create: `config/xiaozhi.ini.example`
  Example runtime config with required keys and comments.
- Create: `tests/test_config.c`
  Config parsing and validation tests.
- Create: `tests/test_event_queue.c`
  Event queue correctness tests.
- Create: `tests/test_audio_buffer.c`
  Audio block queue correctness tests.
- Create: `tests/test_session.c`
  State machine transition tests.
- Create: `tests/test_xiaozhi_protocol.c`
  JSON encode / decode tests.
- Create: `tests/test_xiaozhi_client.c`
  Transport callback and send-queue tests with fake sinks.
- Create: `tests/test_opus_codec.c`
  Opus encode/decode round-trip smoke tests.
- Create: `tests/test_wakeword_snowboy.cc`
  Snowboy wrapper initialization and feed-loop smoke tests.
- Create: `tests/test_app_bootstrap.c`
  App startup and `--check-config` smoke test.

## Chunk 1: Build Foundation and Pure Logic

### Task 1: Reshape the build so the client and tests can evolve safely

**Files:**
- Modify: `Makefile`
- Create: `src/log.h`
- Create: `src/log.c`
- Create: `tests/test_build_smoke.c`

- [ ] **Step 1: Write the first failing smoke test**

```c
#include "log.h"

int main(void) {
  log_info("smoke");
  return 0;
}
```

- [ ] **Step 2: Run the smoke test target and verify it fails**

Run: `make test TEST=test_build_smoke`
Expected: FAIL because `Makefile` has no `test` target and `log.h` does not exist yet.

- [ ] **Step 3: Replace wildcard source discovery with explicit client/test build rules**

Add to `Makefile`:

```makefile
CC := gcc
CXX := g++
TARGET := $(BUILD_DIR)/xiaozhi_client
APP_SRCS := src/main.c src/ws_server.c src/http_server.c src/command.c \
            src/msg_queue.c src/sensor_sim.c src/cJSON.c src/log.c
CPP_SRCS :=
TEST_DIR := tests
```

Also add:

- object rules for `.c` and `.cc`
- dependency flags for `libwebsockets`, `opus`, `alsa`, `pthread`, `libstdc++`
- `make test TEST=<name>` that builds and runs `build/tests/<name>`
- a comment that `APP_SRCS` will be updated task-by-task as the new client modules land, then the old server files will be removed from the app target in Task 10

- [ ] **Step 4: Add the minimal logging module**

`src/log.h`

```c
typedef enum {
  LOG_LEVEL_ERROR = 0,
  LOG_LEVEL_WARN,
  LOG_LEVEL_INFO,
  LOG_LEVEL_DEBUG
} log_level_t;

void log_set_level(log_level_t level);
void log_info(const char *fmt, ...);
void log_warn(const char *fmt, ...);
void log_error(const char *fmt, ...);
```

`src/log.c`

```c
static void log_write(log_level_t level, const char *fmt, va_list ap);
```

- [ ] **Step 5: Re-run the smoke test and verify it passes**

Run: `make test TEST=test_build_smoke`
Expected: PASS and the command exits with status `0`.

- [ ] **Step 6: Verify the main app target still builds as a stub**

Run: `make clean && make`
Expected: `build/xiaozhi_client` is produced from the existing server-oriented sources plus the new logging module.

- [ ] **Step 7: Commit the scaffold**

```bash
git add Makefile src/log.h src/log.c tests/test_build_smoke.c
git commit -m "build: add explicit client and test targets"
```

### Task 2: Add config loading, validation, and example config

**Files:**
- Create: `src/config.h`
- Create: `src/config.c`
- Create: `config/xiaozhi.ini.example`
- Create: `tests/test_config.c`
- Modify: `Makefile`

- [ ] **Step 1: Write failing config tests**

`tests/test_config.c`

```c
static void test_config_requires_server_token(void) {
  const char *ini =
      "[server]\n"
      "url=wss://api.tenclass.net/xiaozhi/v1/\n";
  app_config_t cfg = {0};
  char err[128];
  assert(config_load_from_string(ini, &cfg, err, sizeof(err)) != 0);
  assert(strstr(err, "token") != NULL);
}

static void test_config_applies_defaults(void) {
  const char *ini =
      "[server]\n"
      "url=wss://api.tenclass.net/xiaozhi/v1/\n"
      "token=test-token\n"
      "[snowboy]\n"
      "model=./hixiaozhi.pmdl\n";
  app_config_t cfg = {0};
  char err[128];
  assert(config_load_from_string(ini, &cfg, err, sizeof(err)) == 0);
  assert(cfg.server.protocol_version == 1);
  assert(cfg.audio.input_sample_rate == 16000);
}
```

- [ ] **Step 2: Run the config tests and verify they fail**

Run: `make test TEST=test_config`
Expected: FAIL because `config.h` / `config.c` do not exist yet.

- [ ] **Step 3: Implement the config structs and string-based loader**

`src/config.h`

```c
typedef struct {
  char url[256];
  char token[256];
  int protocol_version;
} server_config_t;

typedef struct {
  char device_id[64];
  char client_id[64];
} device_config_t;

typedef struct {
  char capture_device[64];
  char playback_device[64];
  int input_sample_rate;
  int silence_timeout_ms;
  int max_utterance_ms;
} audio_config_t;

typedef struct {
  char resource_path[256];
  char model_path[256];
  float sensitivity;
  float audio_gain;
} snowboy_config_t;

typedef struct {
  server_config_t server;
  device_config_t device;
  audio_config_t audio;
  snowboy_config_t snowboy;
} app_config_t;

int config_load_file(const char *path, app_config_t *out, char *err, size_t err_size);
int config_load_from_string(const char *ini, app_config_t *out, char *err, size_t err_size);
```

`src/config.c`

- parse only the sections required by the approved spec
- apply defaults for protocol version, sample rate, silence timeout, and max utterance
- build `Authorization: Bearer <token>` later in the client, not in config

- [ ] **Step 4: Add the example config file**

`config/xiaozhi.ini.example`

```ini
[server]
url = wss://api.tenclass.net/xiaozhi/v1/
token = replace-me
protocol_version = 1

[snowboy]
resource = lib/snowboy/snowboy/resources/common.res
model = ./hixiaozhi.pmdl
```

- [ ] **Step 5: Re-run the config tests and verify they pass**

Run: `make test TEST=test_config`
Expected: PASS with exit status `0`.

- [ ] **Step 6: Build the main binary again**

Run: `make`
Expected: PASS with `build/xiaozhi_client` updated.

- [ ] **Step 7: Commit the config slice**

```bash
git add src/config.h src/config.c config/xiaozhi.ini.example tests/test_config.c Makefile
git commit -m "feat: add runtime config loading"
```

### Task 3: Add thread-safe queues for events and audio blocks

**Files:**
- Create: `src/event_queue.h`
- Create: `src/event_queue.c`
- Create: `src/audio_buffer.h`
- Create: `src/audio_buffer.c`
- Create: `tests/test_event_queue.c`
- Create: `tests/test_audio_buffer.c`
- Modify: `Makefile`

- [ ] **Step 1: Write failing queue tests**

`tests/test_event_queue.c`

```c
int main(void) {
  event_queue_t q;
  app_event_t ev = {.type = APP_EVENT_WAKEWORD_DETECTED};
  assert(event_queue_init(&q, 4) == 0);
  assert(event_queue_push(&q, &ev) == 0);
  memset(&ev, 0, sizeof(ev));
  assert(event_queue_pop(&q, &ev, 0) == 1);
  assert(ev.type == APP_EVENT_WAKEWORD_DETECTED);
  event_queue_destroy(&q);
  return 0;
}
```

`tests/test_audio_buffer.c`

```c
int main(void) {
  audio_buffer_t q;
  uint8_t frame[32] = {1, 2, 3};
  assert(audio_buffer_init(&q, 2, 32) == 0);
  assert(audio_buffer_push(&q, frame, sizeof(frame), 16000) == 0);
  assert(audio_buffer_size(&q) == 1);
  audio_buffer_destroy(&q);
  return 0;
}
```

- [ ] **Step 2: Run the queue tests and verify they fail**

Run: `make test TEST=test_event_queue`
Expected: FAIL because `event_queue.h` does not exist yet.

Run: `make test TEST=test_audio_buffer`
Expected: FAIL because `audio_buffer.h` does not exist yet.

- [ ] **Step 3: Implement the queue types with clear responsibilities**

`src/event_queue.h`

```c
typedef enum {
  APP_EVENT_NONE = 0,
  APP_EVENT_WAKEWORD_DETECTED,
  APP_EVENT_WS_CONNECTED,
  APP_EVENT_WS_HELLO,
  APP_EVENT_WS_TEXT,
  APP_EVENT_WS_BINARY,
  APP_EVENT_CAPTURE_SILENCE_TIMEOUT,
  APP_EVENT_PLAYBACK_FINISHED,
  APP_EVENT_ERROR,
  APP_EVENT_SHUTDOWN
} app_event_type_t;
```

`src/audio_buffer.h`

```c
typedef struct {
  uint8_t *data;
  size_t len;
  int sample_rate;
} audio_block_t;
```

Keep `event_queue` for control flow only and `audio_buffer` for binary payloads only.

- [ ] **Step 4: Re-run the queue tests and verify they pass**

Run: `make test TEST=test_event_queue`
Expected: PASS.

Run: `make test TEST=test_audio_buffer`
Expected: PASS.

- [ ] **Step 5: Rebuild the main binary**

Run: `make`
Expected: PASS.

- [ ] **Step 6: Commit the queue layer**

```bash
git add src/event_queue.h src/event_queue.c src/audio_buffer.h src/audio_buffer.c \
        tests/test_event_queue.c tests/test_audio_buffer.c Makefile
git commit -m "feat: add event and audio queues"
```

### Task 4: Add the pure session state machine before touching network or audio threads

**Files:**
- Create: `src/session.h`
- Create: `src/session.c`
- Create: `tests/test_session.c`
- Modify: `Makefile`

- [ ] **Step 1: Write failing session transition tests**

`tests/test_session.c`

```c
int main(void) {
  session_t s;
  session_init(&s);

  assert(s.state == SESSION_STATE_WAKE_DETECTING);
  assert(session_handle_event(&s, APP_EVENT_WAKEWORD_DETECTED) == SESSION_ACTION_CONNECT);
  assert(s.state == SESSION_STATE_CONNECTING);
  assert(session_handle_event(&s, APP_EVENT_WS_HELLO) == SESSION_ACTION_START_LISTEN);
  assert(s.state == SESSION_STATE_READY);
  return 0;
}
```

- [ ] **Step 2: Run the session tests and verify they fail**

Run: `make test TEST=test_session`
Expected: FAIL because `session.h` does not exist yet.

- [ ] **Step 3: Implement the session state machine as pure logic**

`src/session.h`

```c
typedef enum {
  SESSION_STATE_IDLE = 0,
  SESSION_STATE_WAKE_DETECTING,
  SESSION_STATE_CONNECTING,
  SESSION_STATE_HANDSHAKING,
  SESSION_STATE_READY,
  SESSION_STATE_UPLOADING_AUDIO,
  SESSION_STATE_WAITING_TTS,
  SESSION_STATE_PLAYING_TTS,
  SESSION_STATE_ERROR_BACKOFF
} session_state_t;

typedef enum {
  SESSION_ACTION_NONE = 0,
  SESSION_ACTION_CONNECT,
  SESSION_ACTION_SEND_HELLO,
  SESSION_ACTION_START_LISTEN,
  SESSION_ACTION_STOP_LISTEN,
  SESSION_ACTION_ABORT,
  SESSION_ACTION_START_PLAYBACK,
  SESSION_ACTION_RETURN_TO_WAKE
} session_action_t;
```

Keep `session.c` free of ALSA, libwebsockets, or Snowboy code. It should only consume events and emit requested actions.

- [ ] **Step 4: Add negative-path coverage**

Extend `tests/test_session.c` to cover:

- unexpected `APP_EVENT_WS_BINARY` while uploading
- `APP_EVENT_ERROR` from any active state
- barge-in while `SESSION_STATE_PLAYING_TTS`

- [ ] **Step 5: Re-run the session tests and verify they pass**

Run: `make test TEST=test_session`
Expected: PASS.

- [ ] **Step 6: Run all Chunk 1 tests together**

Run: `make test TEST=test_build_smoke && make test TEST=test_config && make test TEST=test_event_queue && make test TEST=test_audio_buffer && make test TEST=test_session`
Expected: all commands PASS.

- [ ] **Step 7: Commit the state machine**

```bash
git add src/session.h src/session.c tests/test_session.c Makefile
git commit -m "feat: add session state machine"
```

### Task 5: Add protocol serialization and parsing as a separate unit

**Files:**
- Create: `src/xiaozhi_protocol.h`
- Create: `src/xiaozhi_protocol.c`
- Create: `tests/test_xiaozhi_protocol.c`
- Modify: `Makefile`

- [ ] **Step 1: Write failing protocol tests**

`tests/test_xiaozhi_protocol.c`

```c
int main(void) {
  char json[256];
  xiaozhi_hello_config_t hello = {
      .protocol_version = 1,
      .sample_rate = 16000,
      .channels = 1,
      .frame_duration_ms = 60};

  assert(xiaozhi_build_hello(&hello, json, sizeof(json)) == 0);
  assert(strstr(json, "\"type\":\"hello\"") != NULL);

  const char *incoming = "{\"type\":\"tts\",\"state\":\"stop\",\"session_id\":\"abc\"}";
  xiaozhi_incoming_event_t event = {0};
  assert(xiaozhi_parse_incoming_json(incoming, &event) == 0);
  assert(event.type == XIAOZHI_EVENT_TTS_STOP);
  return 0;
}
```

- [ ] **Step 2: Run the protocol tests and verify they fail**

Run: `make test TEST=test_xiaozhi_protocol`
Expected: FAIL because `xiaozhi_protocol.h` does not exist yet.

- [ ] **Step 3: Implement outgoing JSON builders**

Add functions to `src/xiaozhi_protocol.h`:

```c
int xiaozhi_build_hello(const xiaozhi_hello_config_t *cfg, char *buf, size_t buf_size);
int xiaozhi_build_listen_detect(char *buf, size_t buf_size);
int xiaozhi_build_listen_start(char *buf, size_t buf_size);
int xiaozhi_build_listen_stop(char *buf, size_t buf_size);
int xiaozhi_build_abort(const char *reason, char *buf, size_t buf_size);
```

- [ ] **Step 4: Implement incoming JSON parsing**

Parse only the approved one-stage set:

- `hello`
- `stt`
- `llm`
- `tts`
- `system`
- `mcp`
- `iot`

Return a compact enum + payload struct rather than passing raw `cJSON *` through the app.

- [ ] **Step 5: Re-run the protocol tests and verify they pass**

Run: `make test TEST=test_xiaozhi_protocol`
Expected: PASS.

- [ ] **Step 6: Re-run all Chunk 1 tests**

Run: `make test TEST=test_build_smoke && make test TEST=test_config && make test TEST=test_event_queue && make test TEST=test_audio_buffer && make test TEST=test_session && make test TEST=test_xiaozhi_protocol`
Expected: all commands PASS.

- [ ] **Step 7: Commit the protocol layer**

```bash
git add src/xiaozhi_protocol.h src/xiaozhi_protocol.c tests/test_xiaozhi_protocol.c Makefile
git commit -m "feat: add xiaozhi protocol codec"
```

## Chunk 2: Runtime Integration and End-to-End Flow

### Task 6: Add the WebSocket client transport around libwebsockets

**Files:**
- Create: `src/xiaozhi_client.h`
- Create: `src/xiaozhi_client.c`
- Create: `tests/test_xiaozhi_client.c`
- Modify: `Makefile`

- [ ] **Step 1: Write failing transport tests around a fake sink**

`tests/test_xiaozhi_client.c`

```c
int main(void) {
  xiaozhi_client_t client;
  xiaozhi_client_config_t cfg = {.url = "wss://example.invalid/ws", .protocol_version = 1};
  xiaozhi_client_callbacks_t cb = {0};

  assert(xiaozhi_client_init(&client, &cfg, &cb) == 0);
  assert(xiaozhi_client_queue_text(&client, "{\"type\":\"hello\"}") == 0);
  assert(xiaozhi_client_pending_text_count(&client) == 1);
  xiaozhi_client_destroy(&client);
  return 0;
}
```

- [ ] **Step 2: Run the transport tests and verify they fail**

Run: `make test TEST=test_xiaozhi_client`
Expected: FAIL because `xiaozhi_client.h` does not exist yet.

- [ ] **Step 3: Implement the transport API with clear callback boundaries**

`src/xiaozhi_client.h`

```c
typedef struct {
  void (*on_connected)(void *ctx);
  void (*on_disconnected)(void *ctx, int code);
  void (*on_text)(void *ctx, const char *payload);
  void (*on_binary)(void *ctx, const uint8_t *data, size_t len);
  void (*on_error)(void *ctx, const char *message);
} xiaozhi_client_callbacks_t;
```

Responsibilities for `src/xiaozhi_client.c`:

- inject required headers
- own outgoing text and binary queues
- convert `libwebsockets` callbacks into the callback table above
- keep reconnect timing out of the pure `session` module

- [ ] **Step 4: Add tests for send queue ordering and disconnect reset**

Extend `tests/test_xiaozhi_client.c` to verify:

- queued hello leaves before queued listen
- pending queues clear on destroy
- empty URL is rejected at init time

- [ ] **Step 5: Re-run the transport tests and verify they pass**

Run: `make test TEST=test_xiaozhi_client`
Expected: PASS.

- [ ] **Step 6: Rebuild the main binary**

Run: `make`
Expected: PASS.

- [ ] **Step 7: Commit the transport layer**

```bash
git add src/xiaozhi_client.h src/xiaozhi_client.c tests/test_xiaozhi_client.c Makefile
git commit -m "feat: add websocket client transport"
```

### Task 7: Add Opus encode/decode and the playback buffer path

**Files:**
- Create: `src/opus_codec.h`
- Create: `src/opus_codec.c`
- Create: `src/audio_playback.h`
- Create: `src/audio_playback.c`
- Create: `tests/test_opus_codec.c`
- Modify: `Makefile`

- [ ] **Step 1: Write failing Opus tests**

`tests/test_opus_codec.c`

```c
int main(void) {
  opus_encoder_wrapper_t enc;
  opus_decoder_wrapper_t dec;
  int16_t pcm[960] = {0};
  uint8_t packet[256];
  int16_t decoded[1920];
  int packet_len;

  assert(opus_encoder_wrapper_init(&enc, 16000, 1, 60) == 0);
  assert(opus_decoder_wrapper_init(&dec, 24000, 1) == 0);
  packet_len = opus_encode_frame(&enc, pcm, 960, packet, sizeof(packet));
  assert(packet_len > 0);
  assert(opus_decode_frame(&dec, packet, packet_len, decoded, 1920) >= 0);
  return 0;
}
```

- [ ] **Step 2: Run the Opus tests and verify they fail**

Run: `make test TEST=test_opus_codec`
Expected: FAIL because `opus_codec.h` does not exist yet.

- [ ] **Step 3: Implement the codec wrapper**

`src/opus_codec.h`

```c
int opus_encoder_wrapper_init(opus_encoder_wrapper_t *enc, int sample_rate, int channels, int frame_ms);
int opus_encode_frame(opus_encoder_wrapper_t *enc, const int16_t *pcm, int samples_per_channel, uint8_t *out, size_t out_size);
int opus_decoder_wrapper_init(opus_decoder_wrapper_t *dec, int sample_rate, int channels);
int opus_decode_frame(opus_decoder_wrapper_t *dec, const uint8_t *packet, size_t packet_len, int16_t *pcm_out, int pcm_capacity);
```

- [ ] **Step 4: Implement playback buffering separately from ALSA device I/O**

`src/audio_playback.h`

```c
int audio_playback_init(audio_playback_t *pb, const audio_playback_config_t *cfg, event_queue_t *events);
int audio_playback_enqueue(audio_playback_t *pb, const int16_t *pcm, size_t frames, int sample_rate);
void audio_playback_request_stop(audio_playback_t *pb);
void audio_playback_destroy(audio_playback_t *pb);
```

Keep playback queueing, thread lifecycle, and ALSA writes together here; do not put protocol parsing into playback.

- [ ] **Step 5: Re-run the Opus tests and add a playback smoke check**

Run: `make test TEST=test_opus_codec`
Expected: PASS.

Run: `make`
Expected: PASS.

- [ ] **Step 6: Commit the playback and codec slice**

```bash
git add src/opus_codec.h src/opus_codec.c src/audio_playback.h src/audio_playback.c \
        tests/test_opus_codec.c Makefile
git commit -m "feat: add opus codec and playback pipeline"
```

### Task 8: Add ALSA capture and local silence timeout handling

**Files:**
- Create: `src/audio_capture.h`
- Create: `src/audio_capture.c`
- Modify: `src/session.h`
- Modify: `src/session.c`
- Modify: `Makefile`

- [ ] **Step 1: Write the first failing capture-side test around silence detection**

Add to `tests/test_session.c`:

```c
assert(session_handle_event(&s, APP_EVENT_CAPTURE_SILENCE_TIMEOUT) == SESSION_ACTION_STOP_LISTEN);
```

- [ ] **Step 2: Re-run the session test and verify it fails**

Run: `make test TEST=test_session`
Expected: FAIL because silence-timeout handling is not implemented yet.

- [ ] **Step 3: Extend the session state machine for local stop conditions**

Update `src/session.c` so that:

- silence timeout during `SESSION_STATE_UPLOADING_AUDIO` emits `SESSION_ACTION_STOP_LISTEN`
- silence timeout in any non-recording state emits `SESSION_ACTION_NONE`

- [ ] **Step 4: Implement the capture thread**

`src/audio_capture.h`

```c
typedef void (*audio_capture_pcm_cb)(void *ctx, const int16_t *pcm, size_t frames);

int audio_capture_start(audio_capture_t *cap, const audio_capture_config_t *cfg,
                        audio_capture_pcm_cb cb, void *cb_ctx,
                        event_queue_t *events);
void audio_capture_set_uploading(audio_capture_t *cap, int enabled);
void audio_capture_stop(audio_capture_t *cap);
```

`src/audio_capture.c`

- open ALSA capture device at `16k / mono / s16le`
- call the PCM callback for each chunk
- track silence duration and push `APP_EVENT_CAPTURE_SILENCE_TIMEOUT` only when `audio_capture_set_uploading(..., 1)` is active

- [ ] **Step 5: Re-run the updated session test and rebuild**

Run: `make test TEST=test_session`
Expected: PASS.

Run: `make`
Expected: PASS.

- [ ] **Step 6: Commit the capture path**

```bash
git add src/audio_capture.h src/audio_capture.c src/session.h src/session.c Makefile tests/test_session.c
git commit -m "feat: add audio capture and silence timeout"
```

### Task 9: Add the Snowboy adapter with a narrow C-facing API

**Files:**
- Create: `src/wakeword_snowboy.h`
- Create: `src/wakeword_snowboy.cc`
- Create: `tests/test_wakeword_snowboy.cc`
- Modify: `Makefile`

- [ ] **Step 1: Write failing Snowboy wrapper tests**

`tests/test_wakeword_snowboy.cc`

```cpp
extern "C" {
#include "wakeword_snowboy.h"
}

int main() {
  wakeword_snowboy_t detector;
  int16_t silence[1600] = {0};
  assert(wakeword_snowboy_init(&detector,
                               "lib/snowboy/snowboy/resources/common.res",
                               "./hixiaozhi.pmdl",
                               0.5f,
                               1.0f) == 0);
  assert(wakeword_snowboy_feed(&detector, silence, 1600) <= 0);
  wakeword_snowboy_destroy(&detector);
  return 0;
}
```

- [ ] **Step 2: Run the Snowboy tests and verify they fail**

Run: `make test TEST=test_wakeword_snowboy`
Expected: FAIL because `wakeword_snowboy.h` does not exist yet.

- [ ] **Step 3: Implement the C wrapper**

`src/wakeword_snowboy.h`

```c
typedef struct wakeword_snowboy wakeword_snowboy_t;

int wakeword_snowboy_init(wakeword_snowboy_t *detector, const char *resource_path,
                          const char *model_path, float sensitivity, float audio_gain);
int wakeword_snowboy_feed(wakeword_snowboy_t *detector, const int16_t *pcm, size_t samples);
void wakeword_snowboy_destroy(wakeword_snowboy_t *detector);
```

Implementation rules:

- keep all Snowboy C++ types private to `.cc`
- do not let C headers include `<string>` or any C++-only constructs
- return `1` on wake-word hit, `0` on no hit, `<0` on error

- [ ] **Step 4: Re-run the Snowboy tests and rebuild**

Run: `make test TEST=test_wakeword_snowboy`
Expected: PASS.

Run: `make`
Expected: PASS.

- [ ] **Step 5: Commit the wake-word adapter**

```bash
git add src/wakeword_snowboy.h src/wakeword_snowboy.cc tests/test_wakeword_snowboy.cc Makefile
git commit -m "feat: add snowboy wake word adapter"
```

### Task 10: Wire the runtime together and replace the old server entrypoint

**Files:**
- Create: `src/app.h`
- Create: `src/app.c`
- Modify: `src/main.c`
- Modify: `src/xiaozhi_client.c`
- Modify: `src/audio_capture.c`
- Modify: `src/audio_playback.c`
- Modify: `src/session.c`
- Modify: `Makefile`

- [ ] **Step 1: Write a failing integration smoke test for app startup without hardware side effects**

Create `tests/test_app_bootstrap.c`

```c
int main(void) {
  app_runtime_t app;
  app_options_t opts = {.config_path = "config/xiaozhi.ini.example", .check_only = 1};
  assert(app_init(&app, &opts) == 0);
  app_destroy(&app);
  return 0;
}
```

- [ ] **Step 2: Run the bootstrap test and verify it fails**

Run: `make test TEST=test_app_bootstrap`
Expected: FAIL because `app.h` does not exist yet.

- [ ] **Step 3: Implement the app bootstrap layer**

`src/app.h`

```c
typedef struct {
  const char *config_path;
  int check_only;
} app_options_t;

int app_init(app_runtime_t *app, const app_options_t *opts);
int app_run(app_runtime_t *app);
void app_destroy(app_runtime_t *app);
```

`src/app.c` responsibilities:

- load and validate config
- set log level
- initialize session, queues, client, wake-word, capture, playback, and codec
- route callbacks into `event_queue`
- drive `session_action_t` decisions into transport/audio side effects
- open the WebSocket only after `SESSION_ACTION_CONNECT`, and close it after `tts/stop` plus playback drain to preserve the approved per-round connection model
- replace the temporary `APP_SRCS` list in `Makefile` with the final client-only source list and set `CPP_SRCS := src/wakeword_snowboy.cc`
- on `check_only`, validate config and dependencies without connecting or opening ALSA devices

- [ ] **Step 4: Replace `src/main.c` with a CLI entrypoint**

Implement:

```c
int main(int argc, char **argv) {
  app_options_t opts = parse_args(argc, argv);
  app_runtime_t app;
  if (app_init(&app, &opts) != 0) return 1;
  return opts.check_only ? 0 : app_run(&app);
}
```

Support:

- `--config <path>`
- `--check-config`

- [ ] **Step 5: Re-run the bootstrap test, the full test suite, and the app build**

Run: `make test TEST=test_app_bootstrap`
Expected: PASS.

Run: `make test TEST=test_build_smoke && make test TEST=test_config && make test TEST=test_event_queue && make test TEST=test_audio_buffer && make test TEST=test_session && make test TEST=test_xiaozhi_protocol && make test TEST=test_xiaozhi_client && make test TEST=test_opus_codec && make test TEST=test_wakeword_snowboy && make test TEST=test_app_bootstrap`
Expected: all commands PASS.

Run: `make clean && make`
Expected: PASS and `build/xiaozhi_client` exists.

- [ ] **Step 6: Run configuration-only verification**

Run: `./build/xiaozhi_client --config config/xiaozhi.ini.example --check-config`
Expected: exits `0` after printing config validation success.

- [ ] **Step 7: Prepare the real runtime config**

Run: `cp config/xiaozhi.ini.example config/xiaozhi.ini`
Expected: local runtime config file exists and can be edited with the real token, optional stable IDs, and the desired Snowboy model path.

- [ ] **Step 8: Run the manual end-to-end smoke test**

Run: `./build/xiaozhi_client --config config/xiaozhi.ini`
Expected:

- startup log reaches `WAKE_DETECTING`
- speaking the Snowboy wake word prints a wake-word hit
- connection to `xiaozhi.me` succeeds
- `hello` and `listen` messages are logged
- speaking a short query uploads audio and eventually prints `stt` / `llm`
- TTS audio is heard and playback completion returns the app to `WAKE_DETECTING`

- [ ] **Step 9: Exercise failure paths manually**

Run these one at a time:

- `./build/xiaozhi_client --config bad-token.ini`
- start another process that holds the capture device, then run the client
- disconnect network during playback

Expected:

- bad token: clear auth failure log, clean exit or backoff
- busy microphone: clear ALSA error without deadlock
- network drop: transport error log, state returns to backoff then wake-detecting

- [ ] **Step 10: Commit the integrated client**

```bash
git add src/app.h src/app.c src/main.c src/xiaozhi_client.c src/audio_capture.c \
        src/audio_playback.c src/session.c Makefile tests/test_app_bootstrap.c
git commit -m "feat: wire xiaozhi voice client runtime"
```
