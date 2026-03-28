#ifndef APP_H
#define APP_H

#include "audio_capture.h"
#include "audio_playback.h"
#include "config.h"
#include "event_queue.h"
#include "opus_codec.h"
#include "ota_client.h"
#include "session.h"
#include "xiaozhi_client.h"
#include "xiaozhi_protocol.h"

typedef struct {
	const char *config_path;
	int check_only;
	int skip_runtime_init;
	ota_fetch_fn_t ota_fetch;
	void *ota_fetch_ctx;
} app_options_t;

typedef enum {
	APP_OBSERVER_EVENT_NONE = 0,
	APP_OBSERVER_EVENT_STATE_CHANGED,
	APP_OBSERVER_EVENT_PROTOCOL,
	APP_OBSERVER_EVENT_ERROR
} app_observer_event_kind_t;

typedef struct {
	app_observer_event_kind_t kind;
	session_state_t state;
	xiaozhi_incoming_event_t protocol;
	int code;
	char text[1024];
} app_observer_event_t;

typedef void (*app_observer_fn)(const app_observer_event_t *ev, void *ctx);

typedef struct {
	app_options_t options;
	app_config_t config;
	event_queue_t events;
	session_t session;
	xiaozhi_client_t client;
	opus_encoder_wrapper_t encoder;
	opus_decoder_wrapper_t decoder;
	audio_capture_t capture;
	audio_playback_t playback;
	ota_response_t ota_response;
	int initialized;
	int check_only;
	int skip_runtime_init;
	int stop_requested;
	int client_started;
	int runtime_modules_initialized;
	int activation_pending;
	int upload_enabled;
	int decoder_sample_rate;
	int tts_done;
	char session_id[64];
	app_observer_fn observer_fn;
	void *observer_ctx;
	session_state_t observer_last_state;
	int observer_state_valid;
} app_runtime_t;

int app_init(app_runtime_t *app, const app_options_t *opts);
int app_run(app_runtime_t *app);
int app_set_observer(app_runtime_t *app, app_observer_fn fn, void *ctx);
int app_control_connect(app_runtime_t *app);
int app_control_disconnect(app_runtime_t *app);
int app_control_set_server_config(app_runtime_t *app, const char *payload_json);
int app_control_set_audio_config(app_runtime_t *app, const char *payload_json);
int app_control_test_connection(app_runtime_t *app);
int app_control_shutdown(app_runtime_t *app);
void app_request_stop(app_runtime_t *app);
void app_destroy(app_runtime_t *app);

#endif /* APP_H */
