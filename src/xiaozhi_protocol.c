#include "xiaozhi_protocol.h"

#include <stdio.h>
#include <string.h>

#include "cJSON.h"

static int print_json(cJSON *root, char *buf, size_t buf_size)
{
	char *printed;

	printed = cJSON_PrintUnformatted(root);
	if (!printed)
		return -1;

	snprintf(buf, buf_size, "%s", printed);
	cJSON_free(printed);
	return 0;
}

int xiaozhi_build_hello(const xiaozhi_hello_config_t *cfg, char *buf, size_t buf_size)
{
	cJSON *root;
	cJSON *audio_params;
	int rc;

	if (!cfg || !buf || buf_size == 0)
		return -1;

	root = cJSON_CreateObject();
	audio_params = cJSON_CreateObject();
	if (!root || !audio_params) {
		cJSON_Delete(root);
		cJSON_Delete(audio_params);
		return -1;
	}

	cJSON_AddStringToObject(root, "type", "hello");
	cJSON_AddNumberToObject(root, "version", cfg->protocol_version);
	cJSON_AddStringToObject(root, "transport", "websocket");
	cJSON_AddStringToObject(audio_params, "format", "opus");
	cJSON_AddNumberToObject(audio_params, "sample_rate", cfg->sample_rate);
	cJSON_AddNumberToObject(audio_params, "channels", cfg->channels);
	cJSON_AddNumberToObject(audio_params, "frame_duration", cfg->frame_duration_ms);
	cJSON_AddItemToObject(root, "audio_params", audio_params);

	rc = print_json(root, buf, buf_size);
	cJSON_Delete(root);
	return rc;
}

int xiaozhi_build_listen_detect(char *buf, size_t buf_size)
{
	return snprintf(buf, buf_size, "{\"type\":\"listen\",\"state\":\"detect\"}") >= (int)buf_size ? -1 : 0;
}

int xiaozhi_build_listen_start(char *buf, size_t buf_size)
{
	return snprintf(buf, buf_size, "{\"type\":\"listen\",\"state\":\"start\",\"mode\":\"auto\"}") >= (int)buf_size ? -1 : 0;
}

int xiaozhi_build_listen_stop(char *buf, size_t buf_size)
{
	return snprintf(buf, buf_size, "{\"type\":\"listen\",\"state\":\"stop\"}") >= (int)buf_size ? -1 : 0;
}

int xiaozhi_build_listen_start_manual(const char *session_id, char *buf, size_t buf_size)
{
	if (!session_id || session_id[0] == '\0' || !buf || buf_size == 0)
		return -1;

	return snprintf(buf, buf_size,
			"{\"session_id\":\"%s\",\"type\":\"listen\",\"state\":\"start\",\"mode\":\"manual\"}",
			session_id) >= (int)buf_size ? -1 : 0;
}

int xiaozhi_build_listen_stop_manual(const char *session_id, char *buf, size_t buf_size)
{
	if (!session_id || session_id[0] == '\0' || !buf || buf_size == 0)
		return -1;

	return snprintf(buf, buf_size,
			"{\"session_id\":\"%s\",\"type\":\"listen\",\"state\":\"stop\",\"mode\":\"manual\"}",
			session_id) >= (int)buf_size ? -1 : 0;
}

int xiaozhi_build_abort(const char *reason, char *buf, size_t buf_size)
{
	const char *safe_reason = reason ? reason : "unknown";

	return snprintf(buf, buf_size,
			"{\"type\":\"abort\",\"reason\":\"%s\"}",
			safe_reason) >= (int)buf_size ? -1 : 0;
}

static xiaozhi_event_type_t parse_type(const cJSON *type, const cJSON *state)
{
	if (!cJSON_IsString(type))
		return XIAOZHI_EVENT_UNKNOWN;

	if (strcmp(type->valuestring, "hello") == 0)
		return XIAOZHI_EVENT_HELLO;
	if (strcmp(type->valuestring, "stt") == 0)
		return XIAOZHI_EVENT_STT;
	if (strcmp(type->valuestring, "llm") == 0)
		return XIAOZHI_EVENT_LLM;
	if (strcmp(type->valuestring, "system") == 0)
		return XIAOZHI_EVENT_SYSTEM;
	if (strcmp(type->valuestring, "mcp") == 0)
		return XIAOZHI_EVENT_MCP;
	if (strcmp(type->valuestring, "iot") == 0)
		return XIAOZHI_EVENT_IOT;
	if (strcmp(type->valuestring, "tts") == 0) {
		if (cJSON_IsString(state) && strcmp(state->valuestring, "start") == 0)
			return XIAOZHI_EVENT_TTS_START;
		if (cJSON_IsString(state) && strcmp(state->valuestring, "stop") == 0)
			return XIAOZHI_EVENT_TTS_STOP;
		if (cJSON_IsString(state) &&
		    (strcmp(state->valuestring, "sentence_start") == 0 ||
		     strcmp(state->valuestring, "sentence_end") == 0))
			return XIAOZHI_EVENT_TTS_SENTENCE;
		return XIAOZHI_EVENT_UNKNOWN;
	}

	return XIAOZHI_EVENT_UNKNOWN;
}

static void copy_json_string(char *dst, size_t dst_size, const cJSON *item)
{
	if (!dst || dst_size == 0)
		return;

	dst[0] = '\0';
	if (cJSON_IsString(item))
		snprintf(dst, dst_size, "%s", item->valuestring);
}

int xiaozhi_parse_incoming_json(const char *json, xiaozhi_incoming_event_t *event)
{
	cJSON *root;
	cJSON *type;
	cJSON *state;
	cJSON *session_id;
	cJSON *text;
	cJSON *emotion;
	cJSON *audio_params;
	cJSON *sample_rate;
	cJSON *channels;

	if (!json || !event)
		return -1;

	memset(event, 0, sizeof(*event));
	root = cJSON_Parse(json);
	if (!root)
		return -1;

	type = cJSON_GetObjectItem(root, "type");
	state = cJSON_GetObjectItem(root, "state");
	session_id = cJSON_GetObjectItem(root, "session_id");
	text = cJSON_GetObjectItem(root, "text");
	emotion = cJSON_GetObjectItem(root, "emotion");
	audio_params = cJSON_GetObjectItem(root, "audio_params");
	sample_rate = audio_params ? cJSON_GetObjectItem(audio_params, "sample_rate") : NULL;
	channels = audio_params ? cJSON_GetObjectItem(audio_params, "channels") : NULL;

	event->type = parse_type(type, state);
	copy_json_string(event->session_id, sizeof(event->session_id), session_id);
	copy_json_string(event->text, sizeof(event->text), text);
	copy_json_string(event->emotion, sizeof(event->emotion), emotion);
	if (cJSON_IsNumber(sample_rate))
		event->output_sample_rate = sample_rate->valueint;
	if (cJSON_IsNumber(channels))
		event->output_channels = channels->valueint;

	cJSON_Delete(root);
	return 0;
}
