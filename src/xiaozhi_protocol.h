#ifndef XIAOZHI_PROTOCOL_H
#define XIAOZHI_PROTOCOL_H

#include <stddef.h>

typedef struct {
	int protocol_version;
	int sample_rate;
	int channels;
	int frame_duration_ms;
} xiaozhi_hello_config_t;

typedef enum {
	XIAOZHI_EVENT_NONE = 0,
	XIAOZHI_EVENT_HELLO,
	XIAOZHI_EVENT_STT,
	XIAOZHI_EVENT_LLM,
	XIAOZHI_EVENT_TTS_START,
	XIAOZHI_EVENT_TTS_STOP,
	XIAOZHI_EVENT_SYSTEM,
	XIAOZHI_EVENT_MCP,
	XIAOZHI_EVENT_IOT,
	XIAOZHI_EVENT_UNKNOWN
} xiaozhi_event_type_t;

typedef struct {
	xiaozhi_event_type_t type;
	char session_id[64];
	char text[256];
	char emotion[64];
	int output_sample_rate;
	int output_channels;
} xiaozhi_incoming_event_t;

int xiaozhi_build_hello(const xiaozhi_hello_config_t *cfg, char *buf, size_t buf_size);
int xiaozhi_build_listen_detect(char *buf, size_t buf_size);
int xiaozhi_build_listen_start(char *buf, size_t buf_size);
int xiaozhi_build_listen_stop(char *buf, size_t buf_size);
int xiaozhi_build_listen_start_manual(const char *session_id, char *buf, size_t buf_size);
int xiaozhi_build_listen_stop_manual(const char *session_id, char *buf, size_t buf_size);
int xiaozhi_build_abort(const char *reason, char *buf, size_t buf_size);
int xiaozhi_parse_incoming_json(const char *json, xiaozhi_incoming_event_t *event);

#endif /* XIAOZHI_PROTOCOL_H */
