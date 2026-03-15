#ifndef APP_H
#define APP_H

#include <stdio.h>

#include "audio_capture.h"
#include "audio_playback.h"
#include "config.h"
#include "event_queue.h"
#include "opus_codec.h"
#include "session.h"
#include "xiaozhi_client.h"

typedef struct {
	const char *config_path;
	int check_only;
} app_options_t;

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
	int initialized;
	int check_only;
	int stop_requested;
	int client_started;
	int upload_enabled;
	int decoder_sample_rate;
	int tts_done;
	pthread_mutex_t opus_dump_mutex;
	int opus_dump_mutex_initialized;
	FILE *opus_dump_file;
	int opus_dump_index;
	char opus_dump_path[256];
	char session_id[64];
} app_runtime_t;

int app_init(app_runtime_t *app, const app_options_t *opts);
int app_run(app_runtime_t *app);
void app_request_stop(app_runtime_t *app);
void app_destroy(app_runtime_t *app);

#endif /* APP_H */
