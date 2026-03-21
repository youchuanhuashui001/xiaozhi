#include "app.h"

#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "xiaozhi_protocol.h"

static void app_reset_to_idle(app_runtime_t *app);
static void app_log_manual_ready(void);
static int app_bootstrap_ota(app_runtime_t *app);
static void app_log_activation_required(const app_runtime_t *app);

static void app_log_ws_request_headers(const app_runtime_t *app)
{
	char auth_preview[96];
	size_t token_len;

	if (!app)
		return;

	token_len = strlen(app->config.server.token);
	if (token_len == 0) {
		snprintf(auth_preview, sizeof(auth_preview), "(not set)");
	} else {
		snprintf(auth_preview, sizeof(auth_preview), "Bearer %.24s%s",
			 app->config.server.token, token_len > 24 ? "..." : "");
	}

	log_info("ws request headers:");
	log_info("Authorization: %s", auth_preview);
	log_info("Protocol-Version: %d", app->config.server.protocol_version);
	log_info("Device-Id: %s", app->config.device.device_id);
	log_info("Client-Id: %s", app->config.device.client_id);
}

static int app_open_opus_dump_file(app_runtime_t *app)
{
	FILE *fp;

	if (!app)
		return -1;

	pthread_mutex_lock(&app->opus_dump_mutex);
	if (app->opus_dump_file) {
		fclose(app->opus_dump_file);
		app->opus_dump_file = NULL;
	}

	app->opus_dump_index++;
	snprintf(app->opus_dump_path, sizeof(app->opus_dump_path),
		 "build/upload_%03d.opusbin", app->opus_dump_index);
	fp = fopen(app->opus_dump_path, "wb");
	if (!fp) {
		snprintf(app->opus_dump_path, sizeof(app->opus_dump_path),
			 "/tmp/xiaozhi-upload-%03d.opusbin", app->opus_dump_index);
		fp = fopen(app->opus_dump_path, "wb");
	}
	app->opus_dump_file = fp;
	pthread_mutex_unlock(&app->opus_dump_mutex);

	if (!fp)
		return -1;

	log_info("recording opus upload frames to %s", app->opus_dump_path);
	return 0;
}

static void app_close_opus_dump_file(app_runtime_t *app)
{
	int had_file = 0;

	if (!app || !app->opus_dump_mutex_initialized)
		return;

	pthread_mutex_lock(&app->opus_dump_mutex);
	if (app->opus_dump_file) {
		fclose(app->opus_dump_file);
		app->opus_dump_file = NULL;
		had_file = 1;
	}
	pthread_mutex_unlock(&app->opus_dump_mutex);

	if (had_file)
		log_info("opus upload dump saved: %s", app->opus_dump_path);
}

static int app_write_opus_packet(app_runtime_t *app, const uint8_t *packet, size_t len)
{
	uint8_t header[2];
	FILE *fp;

	if (!app || !packet || len == 0 || len > 65535 || !app->opus_dump_mutex_initialized)
		return -1;

	header[0] = (uint8_t)(len & 0xFF);
	header[1] = (uint8_t)((len >> 8) & 0xFF);

	pthread_mutex_lock(&app->opus_dump_mutex);
	fp = app->opus_dump_file;
	if (!fp) {
		pthread_mutex_unlock(&app->opus_dump_mutex);
		return -1;
	}

	if (fwrite(header, 1, sizeof(header), fp) != sizeof(header) ||
	    fwrite(packet, 1, len, fp) != len) {
		pthread_mutex_unlock(&app->opus_dump_mutex);
		return -1;
	}
	fflush(fp);
	pthread_mutex_unlock(&app->opus_dump_mutex);
	return 0;
}

static void app_push_event(app_runtime_t *app, app_event_type_t type,
			   const char *text, int code)
{
	app_event_t event;

	memset(&event, 0, sizeof(event));
	event.type = type;
	event.code = code;
	if (text)
		snprintf(event.text, sizeof(event.text), "%s", text);
	event.data_len = text ? strlen(event.text) : 0;
	(void)event_queue_push(&app->events, &event);
}

static log_level_t app_parse_log_level(const char *name)
{
	if (!name || strcmp(name, "info") == 0)
		return LOG_LEVEL_INFO;
	if (strcmp(name, "debug") == 0)
		return LOG_LEVEL_DEBUG;
	if (strcmp(name, "warn") == 0)
		return LOG_LEVEL_WARN;
	if (strcmp(name, "error") == 0)
		return LOG_LEVEL_ERROR;
	return LOG_LEVEL_INFO;
}

static void app_fill_default_ids(app_runtime_t *app)
{
	if (app->config.device.device_id[0] == '\0')
		snprintf(app->config.device.device_id,
			 sizeof(app->config.device.device_id),
			 "desktop-device");
	if (app->config.device.client_id[0] == '\0')
		snprintf(app->config.device.client_id,
			 sizeof(app->config.device.client_id),
			 "desktop-client");
}

static void app_log_activation_required(const app_runtime_t *app)
{
	if (!app || !app->activation_pending)
		return;

	log_warn("activation required");
	log_warn("activation code: %s",
		 app->ota_response.activation.code[0] != '\0' ?
		 app->ota_response.activation.code : "(missing)");
	if (app->ota_response.activation.message[0] != '\0')
		log_warn("activation message: %s", app->ota_response.activation.message);
	log_warn("please activate the device, then restart the client");
}

static int app_bootstrap_ota(app_runtime_t *app)
{
	char err[256];
	ota_fetch_fn_t fetcher;

	if (!app)
		return -1;

	fetcher = app->options.ota_fetch ? app->options.ota_fetch : ota_fetch;
	if (fetcher(&app->config, &app->ota_response, err, sizeof(err),
		    app->options.ota_fetch_ctx) != 0) {
		log_error("ota bootstrap failed: %s", err);
		return -1;
	}

	if (app->ota_response.websocket.url[0] != '\0') {
		snprintf(app->config.server.url, sizeof(app->config.server.url), "%s",
			 app->ota_response.websocket.url);
	}
	if (app->ota_response.websocket.token[0] != '\0') {
		snprintf(app->config.server.token, sizeof(app->config.server.token), "%s",
			 app->ota_response.websocket.token);
	}

	if (app->ota_response.activation_required) {
		app->activation_pending = 1;
		return 0;
	}

	if (app->config.server.url[0] == '\0') {
		log_error("ota bootstrap did not provide websocket.url and no fallback server.url is configured");
		return -1;
	}

	return 0;
}

static void app_client_on_connected(void *ctx)
{
	app_runtime_t *app = ctx;

	app_push_event(app, APP_EVENT_WS_CONNECTED, NULL, 0);
}

static void app_client_on_disconnected(void *ctx, int code)
{
	app_runtime_t *app = ctx;
	char message[128];

	snprintf(message, sizeof(message), "websocket disconnected (code=%d)", code);
	if (app->stop_requested) {
		log_info("%s", message);
		return;
	}

	log_warn("%s", message);
	app_push_event(app, APP_EVENT_ERROR, message, code);
}

static void app_client_on_text(void *ctx, const char *payload)
{
	app_runtime_t *app = ctx;

	log_info("ws recv text: %s", payload ? payload : "");
	app_push_event(app, APP_EVENT_WS_TEXT, payload, 0);
}

static void app_client_on_binary(void *ctx, const uint8_t *data, size_t len)
{
	app_runtime_t *app = ctx;
	int16_t pcm[4096];
	int frames;

	if (app->upload_enabled || !data || len == 0)
		return;

	log_info("ws recv binary: %zu bytes", len);
	frames = opus_decode_frame(&app->decoder, data, len, pcm,
				  (int)(sizeof(pcm) / sizeof(pcm[0])));
	if (frames > 0) {
		(void)audio_playback_enqueue(&app->playback, pcm, (size_t)frames,
					     app->decoder_sample_rate);
	}
}

static void app_client_on_error(void *ctx, const char *message)
{
	app_runtime_t *app = ctx;

	log_warn("ws error detail: %s", message ? message : "unknown");
	app_push_event(app, APP_EVENT_ERROR, message, -1);
}

static void app_capture_on_pcm(void *ctx, const int16_t *pcm, size_t frames)
{
	app_runtime_t *app = ctx;

	if (app->upload_enabled) {
		uint8_t packet[512];
		int packet_len;

		packet_len = opus_encode_frame(&app->encoder, pcm, (int)frames,
					      packet, sizeof(packet));
		if (packet_len > 0) {
			if (app_write_opus_packet(app, packet, (size_t)packet_len) != 0)
				log_warn("failed to dump opus frame to file");
			if (xiaozhi_client_queue_binary(&app->client, packet,
							(size_t)packet_len) != 0) {
				log_warn("failed to queue opus frame for websocket upload");
			}
		}
	}
}

static int app_send_hello(app_runtime_t *app)
{
	char payload[512];
	xiaozhi_hello_config_t hello = {
		.protocol_version = app->config.server.protocol_version,
		.sample_rate = app->config.audio.input_sample_rate,
		.channels = 1,
		.frame_duration_ms = 60
	};

	if (xiaozhi_build_hello(&hello, payload, sizeof(payload)) != 0)
		return -1;
	log_info("hello payload: %s", payload);
	return xiaozhi_client_queue_text(&app->client, payload);
}

static int app_send_listen_start(app_runtime_t *app)
{
	char payload[256];

	if (!app || app->session_id[0] == '\0')
		return -1;
	if (xiaozhi_build_listen_start_manual(app->session_id, payload, sizeof(payload)) != 0)
		return -1;
	log_info("listen start payload: %s", payload);
	return xiaozhi_client_queue_text(&app->client, payload);
}

static int app_send_listen_stop(app_runtime_t *app)
{
	char payload[256];

	if (!app || app->session_id[0] == '\0')
		return -1;
	if (xiaozhi_build_listen_stop_manual(app->session_id, payload, sizeof(payload)) != 0)
		return -1;
	log_info("listen stop payload: %s", payload);
	return xiaozhi_client_queue_text(&app->client, payload);
}

static int app_send_abort(app_runtime_t *app)
{
	char payload[256];

	if (xiaozhi_build_abort("manual_interrupt", payload, sizeof(payload)) != 0)
		return -1;
	return xiaozhi_client_queue_text(&app->client, payload);
}

static void app_handle_action(app_runtime_t *app, session_action_t action)
{
	switch (action) {
	case SESSION_ACTION_CONNECT:
		log_info("connecting to xiaozhi");
		app_log_ws_request_headers(app);
		app->session_id[0] = '\0';
		app->tts_done = 0;
		if (xiaozhi_client_start(&app->client) == 0)
			app->client_started = 1;
		else
			app_push_event(app, APP_EVENT_ERROR, "failed to start websocket client", -1);
		break;

	case SESSION_ACTION_SEND_HELLO:
		log_info("websocket connected, sending hello");
		if (app_send_hello(app) != 0)
			app_push_event(app, APP_EVENT_ERROR, "failed to queue hello", -1);
		break;

	case SESSION_ACTION_START_LISTEN:
		log_info("starting audio upload");
		if (app_send_listen_start(app) != 0) {
			app_push_event(app, APP_EVENT_ERROR, "failed to queue listen start", -1);
			break;
		}
		if (app_open_opus_dump_file(app) != 0) {
			app_push_event(app, APP_EVENT_ERROR, "failed to open opus dump file", -1);
			break;
		}
		app->upload_enabled = 1;
		app->session.state = SESSION_STATE_UPLOADING_AUDIO;
		audio_capture_set_uploading(&app->capture, 1);
		break;

	case SESSION_ACTION_STOP_LISTEN:
		log_info("stopping audio upload");
		app->upload_enabled = 0;
		audio_capture_set_uploading(&app->capture, 0);
		app_close_opus_dump_file(app);
		if (app_send_listen_stop(app) != 0)
			app_push_event(app, APP_EVENT_ERROR, "failed to queue listen stop", -1);
		break;

	case SESSION_ACTION_ABORT:
		log_info("interrupting current response");
		app->upload_enabled = 0;
		audio_capture_set_uploading(&app->capture, 0);
		app_close_opus_dump_file(app);
		(void)app_send_abort(app);
		audio_playback_request_stop(&app->playback);
		if (app->client_started)
			xiaozhi_client_stop(&app->client);
		app->client_started = 0;
		app->session_id[0] = '\0';
		if (xiaozhi_client_start(&app->client) == 0)
			app->client_started = 1;
		else
			app_push_event(app, APP_EVENT_ERROR, "failed to restart websocket client", -1);
		break;

	case SESSION_ACTION_RETURN_TO_WAKE:
		if (app->config.runtime.reconnect_backoff_ms > 0)
			usleep((useconds_t)app->config.runtime.reconnect_backoff_ms * 1000U);

		app->upload_enabled = 0;
		app->tts_done = 0;
		audio_capture_set_uploading(&app->capture, 0);
		app_close_opus_dump_file(app);
		app->session_id[0] = '\0';
		if (app->client_started) {
			xiaozhi_client_stop(&app->client);
			app->client_started = 0;
		}
		app->session.state = SESSION_STATE_CONNECTING;
		log_info("reconnecting to xiaozhi");
		if (xiaozhi_client_start(&app->client) == 0)
			app->client_started = 1;
		else
			log_error("failed to restart websocket client");
		break;

	default:
		break;
	}
}

static void app_reset_to_idle(app_runtime_t *app)
{
	app->upload_enabled = 0;
	app->tts_done = 0;
	audio_capture_set_uploading(&app->capture, 0);
	app_close_opus_dump_file(app);
	if (app->client_started && xiaozhi_client_is_connected(&app->client))
		app->session.state = SESSION_STATE_READY;
	else
		app->session.state = SESSION_STATE_IDLE;
	if (app->session.state == SESSION_STATE_READY)
		app_log_manual_ready();
}

static void app_log_manual_ready(void)
{
	log_info("ready for manual input; press Enter to talk, Enter again to stop, q then Enter to quit");
}

static void app_update_decoder_from_hello(app_runtime_t *app,
					  const xiaozhi_incoming_event_t *event)
{
	int sample_rate = event->output_sample_rate > 0 ?
		event->output_sample_rate : app->decoder_sample_rate;
	int channels = event->output_channels > 0 ?
		event->output_channels : 1;

	if (sample_rate == app->decoder_sample_rate && channels == 1)
		return;

	opus_decoder_wrapper_destroy(&app->decoder);
	if (opus_decoder_wrapper_init(&app->decoder, sample_rate, channels) == 0)
		app->decoder_sample_rate = sample_rate;
}

static void app_handle_protocol_event(app_runtime_t *app,
				      const xiaozhi_incoming_event_t *event)
{
	session_action_t action = SESSION_ACTION_NONE;

	switch (event->type) {
	case XIAOZHI_EVENT_HELLO:
		log_info("received hello from server");
		if (event->session_id[0] != '\0') {
			snprintf(app->session_id, sizeof(app->session_id), "%s", event->session_id);
			log_info("active session_id: %s", app->session_id);
		}
		app_update_decoder_from_hello(app, event);
		action = session_handle_event(&app->session, APP_EVENT_WS_HELLO);
		app_handle_action(app, action);
		if (app->session.state == SESSION_STATE_READY)
			app_log_manual_ready();
		break;

	case XIAOZHI_EVENT_STT:
		log_info("stt: %s", event->text);
		break;

	case XIAOZHI_EVENT_LLM:
		log_info("llm: %s", event->text);
		break;

	case XIAOZHI_EVENT_TTS_START:
		log_info("tts started");
		app->upload_enabled = 0;
		audio_capture_set_uploading(&app->capture, 0);
		app->session.state = SESSION_STATE_PLAYING_TTS;
		app->tts_done = 0;
		break;

	case XIAOZHI_EVENT_TTS_STOP:
		log_info("tts stopped");
		app->tts_done = 1;
		if (audio_buffer_size(&app->playback.queue) == 0)
			app_reset_to_idle(app);
		break;

	case XIAOZHI_EVENT_SYSTEM:
		log_warn("system: %s", event->text);
		break;

	case XIAOZHI_EVENT_MCP:
	case XIAOZHI_EVENT_IOT:
		log_warn("received unsupported capability event");
		break;

	case XIAOZHI_EVENT_UNKNOWN:
	default:
		log_warn("received unknown protocol event");
		break;
	}
}

static int app_init_runtime_modules(app_runtime_t *app)
{
	xiaozhi_client_config_t client_cfg;
	xiaozhi_client_callbacks_t callbacks;
	audio_capture_config_t capture_cfg;
	audio_playback_config_t playback_cfg;

	pthread_mutex_init(&app->opus_dump_mutex, NULL);
	app->opus_dump_mutex_initialized = 1;

	memset(&client_cfg, 0, sizeof(client_cfg));
	snprintf(client_cfg.url, sizeof(client_cfg.url), "%s",
		 app->config.server.url);
	client_cfg.protocol_version = app->config.server.protocol_version;
	if (app->config.server.token[0] != '\0') {
		snprintf(client_cfg.authorization, sizeof(client_cfg.authorization),
			 "Bearer %.240s", app->config.server.token);
	} else {
		client_cfg.authorization[0] = '\0';
	}
	snprintf(client_cfg.device_id, sizeof(client_cfg.device_id), "%s",
		 app->config.device.device_id);
	snprintf(client_cfg.client_id, sizeof(client_cfg.client_id), "%s",
		 app->config.device.client_id);

	memset(&callbacks, 0, sizeof(callbacks));
	callbacks.on_connected = app_client_on_connected;
	callbacks.on_disconnected = app_client_on_disconnected;
	callbacks.on_text = app_client_on_text;
	callbacks.on_binary = app_client_on_binary;
	callbacks.on_error = app_client_on_error;

	if (xiaozhi_client_init(&app->client, &client_cfg, &callbacks, app) != 0) {
		log_error("failed to initialize websocket client");
		return -1;
	}

	if (opus_encoder_wrapper_init(&app->encoder,
				      app->config.audio.input_sample_rate, 1, 60) != 0) {
		log_error("failed to initialize opus encoder");
		return -1;
	}

	if (opus_decoder_wrapper_init(&app->decoder, 24000, 1) != 0) {
		log_error("failed to initialize opus decoder");
		return -1;
	}
	app->decoder_sample_rate = 24000;

	memset(&playback_cfg, 0, sizeof(playback_cfg));
	snprintf(playback_cfg.device, sizeof(playback_cfg.device), "%s",
		 app->config.audio.playback_device);
	playback_cfg.channels = 1;
	playback_cfg.default_sample_rate = 24000;
	playback_cfg.queue_capacity = 32;
	playback_cfg.max_frame_bytes = 8192;
	if (audio_playback_init(&app->playback, &playback_cfg, &app->events) != 0) {
		log_error("failed to initialize audio playback");
		return -1;
	}

	memset(&capture_cfg, 0, sizeof(capture_cfg));
	snprintf(capture_cfg.device, sizeof(capture_cfg.device), "%s",
		 app->config.audio.capture_device);
	capture_cfg.sample_rate = app->config.audio.input_sample_rate;
	capture_cfg.channels = 1;
	capture_cfg.period_frames = 960;
	capture_cfg.silence_timeout_ms = app->config.audio.silence_timeout_ms;
	capture_cfg.silence_threshold = app->config.audio.silence_threshold;
	if (audio_capture_start(&app->capture, &capture_cfg,
				 app_capture_on_pcm, app, &app->events) != 0) {
		log_error("failed to initialize audio capture");
		return -1;
	}

	app->runtime_modules_initialized = 1;
	return 0;
}

int app_init(app_runtime_t *app, const app_options_t *opts)
{
	char err[256];
	const char *config_path;

	if (!app)
		return -1;

	memset(app, 0, sizeof(*app));
	if (opts)
		app->options = *opts;

	config_path = app->options.config_path ? app->options.config_path :
		"config/xiaozhi.ini";
	if (config_load_file(config_path, &app->config, err, sizeof(err)) != 0) {
		fprintf(stderr, "config error: %s\n", err);
		return -1;
	}

	log_set_level(app_parse_log_level(app->config.runtime.log_level));
	app_fill_default_ids(app);

	if (event_queue_init(&app->events, 64) != 0)
		return -1;

	session_init(&app->session);
	app->check_only = app->options.check_only;
	app->skip_runtime_init = app->options.skip_runtime_init;
	app->initialized = 1;

	if (app->check_only) {
		log_info("config validation success");
		return 0;
	}

	if (app_bootstrap_ota(app) != 0)
		return -1;
	if (app->activation_pending)
		return 0;
	if (app->skip_runtime_init)
		return 0;

	if (app_init_runtime_modules(app) != 0)
		return -1;

	log_info("runtime initialization success");
	return 0;
}

static void app_handle_manual_line(app_runtime_t *app, const char *line)
{
	if (!app || !line)
		return;

	if (strcmp(line, "q\n") == 0 || strcmp(line, "q\r\n") == 0) {
		app_request_stop(app);
		return;
	}

	if (strcmp(line, "\n") != 0 && strcmp(line, "\r\n") != 0)
		return;

	if (app->session.state == SESSION_STATE_UPLOADING_AUDIO)
		app_push_event(app, APP_EVENT_MANUAL_STOP, NULL, 0);
	else
		app_push_event(app, APP_EVENT_MANUAL_START, NULL, 0);
}

static void app_poll_stdin(app_runtime_t *app)
{
	struct pollfd pfd;
	char line[128];

	if (!app || !isatty(STDIN_FILENO))
		return;

	memset(&pfd, 0, sizeof(pfd));
	pfd.fd = STDIN_FILENO;
	pfd.events = POLLIN;
	if (poll(&pfd, 1, 0) <= 0 || !(pfd.revents & POLLIN))
		return;

	if (!fgets(line, sizeof(line), stdin)) {
		if (feof(stdin))
			app_request_stop(app);
		return;
	}

	app_handle_manual_line(app, line);
}

int app_run(app_runtime_t *app)
{
	if (!app || !app->initialized)
		return -1;

	if (app->activation_pending) {
		app_log_activation_required(app);
		return 1;
	}
	if (app->skip_runtime_init)
		return 0;

	app_reset_to_idle(app);
	if (!app->client_started) {
		app->session.state = SESSION_STATE_CONNECTING;
		app_handle_action(app, SESSION_ACTION_CONNECT);
	}

	while (!app->stop_requested) {
		app_event_t event;
		int rc = event_queue_pop(&app->events, &event, 100);

		if (rc < 0)
			return -1;
		if (rc == 0) {
			app_poll_stdin(app);
			continue;
		}

		switch (event.type) {
		case APP_EVENT_MANUAL_START:
		case APP_EVENT_WAKEWORD_DETECTED:
			app_handle_action(app,
					 session_handle_event(&app->session,
								  event.type));
			break;

		case APP_EVENT_MANUAL_STOP:
			app_handle_action(app,
					 session_handle_event(&app->session,
								  APP_EVENT_MANUAL_STOP));
			break;

		case APP_EVENT_WS_CONNECTED:
			app_handle_action(app,
					 session_handle_event(&app->session,
								  APP_EVENT_WS_CONNECTED));
			break;

		case APP_EVENT_WS_TEXT: {
			xiaozhi_incoming_event_t protocol_event;

			if (xiaozhi_parse_incoming_json(event.text, &protocol_event) == 0)
				app_handle_protocol_event(app, &protocol_event);
			else
				log_warn("invalid protocol json: %s", event.text);
			break;
		}

		case APP_EVENT_CAPTURE_SILENCE_TIMEOUT:
			log_info("silence timeout reached (%d ms, threshold=%d), ignored in manual mode",
				 app->config.audio.silence_timeout_ms,
				 app->config.audio.silence_threshold);
			app_handle_action(app,
					 session_handle_event(&app->session,
								  APP_EVENT_CAPTURE_SILENCE_TIMEOUT));
			break;

		case APP_EVENT_PLAYBACK_FINISHED:
			if (app->tts_done)
				app_reset_to_idle(app);
			break;

		case APP_EVENT_ERROR:
			log_warn("runtime error: %s", event.text);
			app_handle_action(app,
					 session_handle_event(&app->session,
								  APP_EVENT_ERROR));
			break;

		case APP_EVENT_SHUTDOWN:
			app->stop_requested = 1;
			break;

		default:
			break;
		}

		app_poll_stdin(app);
	}

	return 0;
}

void app_request_stop(app_runtime_t *app)
{
	if (!app)
		return;

	app->stop_requested = 1;
	app_push_event(app, APP_EVENT_SHUTDOWN, NULL, 0);
}

void app_destroy(app_runtime_t *app)
{
	if (!app || !app->initialized)
		return;

	if (!app->check_only && app->runtime_modules_initialized) {
		app_close_opus_dump_file(app);
		if (app->client_started)
			xiaozhi_client_stop(&app->client);
		audio_capture_stop(&app->capture);
		audio_playback_destroy(&app->playback);
		opus_encoder_wrapper_destroy(&app->encoder);
		opus_decoder_wrapper_destroy(&app->decoder);
		xiaozhi_client_destroy(&app->client);
	}
	if (app->opus_dump_mutex_initialized) {
		pthread_mutex_destroy(&app->opus_dump_mutex);
		app->opus_dump_mutex_initialized = 0;
	}

	event_queue_destroy(&app->events);
	memset(app, 0, sizeof(*app));
}
