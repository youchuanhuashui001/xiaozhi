#include <assert.h>
#include <string.h>

#include "xiaozhi_protocol.h"

int main(void)
{
	char json[256];
	xiaozhi_hello_config_t hello = {
		.protocol_version = 1,
		.sample_rate = 16000,
		.channels = 1,
		.frame_duration_ms = 60
	};
	xiaozhi_incoming_event_t event = {0};
	const char *incoming = "{\"type\":\"tts\",\"state\":\"stop\",\"session_id\":\"abc\"}";
	const char *incoming_sentence =
		"{\"type\":\"tts\",\"state\":\"sentence_start\",\"text\":\"hello\",\"session_id\":\"abc\"}";

	assert(xiaozhi_build_hello(&hello, json, sizeof(json)) == 0);
	assert(strstr(json, "\"type\":\"hello\"") != NULL);
	assert(xiaozhi_build_listen_start_manual("abc123", json, sizeof(json)) == 0);
	assert(strstr(json, "\"session_id\":\"abc123\"") != NULL);
	assert(strstr(json, "\"state\":\"start\"") != NULL);
	assert(strstr(json, "\"mode\":\"manual\"") != NULL);
	assert(xiaozhi_build_listen_stop_manual("abc123", json, sizeof(json)) == 0);
	assert(strstr(json, "\"state\":\"stop\"") != NULL);
	assert(strstr(json, "\"mode\":\"manual\"") != NULL);

	assert(xiaozhi_parse_incoming_json(incoming, &event) == 0);
	assert(event.type == XIAOZHI_EVENT_TTS_STOP);
	assert(xiaozhi_parse_incoming_json(incoming_sentence, &event) == 0);
	assert(event.type == XIAOZHI_EVENT_TTS_SENTENCE);

	return 0;
}
