#include <assert.h>
#include <string.h>

#include "control_plane/control_events.h"
#include "control_plane/control_protocol.h"
#include "session.h"
#include "xiaozhi_protocol.h"

static void test_hello_maps_to_session_hello(void)
{
	xiaozhi_incoming_event_t in = {0};
	control_event_t out = {0};

	in.type = XIAOZHI_EVENT_HELLO;
	strcpy(in.session_id, "session-1");
	in.output_sample_rate = 16000;
	in.output_channels = 1;

	assert(control_events_from_protocol(&in, &out) == 0);
	assert(strcmp(out.name, "session_hello") == 0);
	assert(strstr(out.payload, "\"session_id\":\"session-1\"") != NULL);
	assert(strstr(out.payload, "\"output_sample_rate\":16000") != NULL);
	assert(strstr(out.payload, "\"output_channels\":1") != NULL);
}

static void test_stt_maps_to_stt_result(void)
{
	xiaozhi_incoming_event_t in = {0};
	control_event_t out = {0};

	in.type = XIAOZHI_EVENT_STT;
	strcpy(in.text, "hello world");

	assert(control_events_from_protocol(&in, &out) == 0);
	assert(strcmp(out.name, "stt_result") == 0);
	assert(strstr(out.payload, "\"text\":\"hello world\"") != NULL);
}

static void test_llm_maps_to_llm_text(void)
{
	xiaozhi_incoming_event_t in = {0};
	control_event_t out = {0};

	in.type = XIAOZHI_EVENT_LLM;
	strcpy(in.text, "reply");

	assert(control_events_from_protocol(&in, &out) == 0);
	assert(strcmp(out.name, "llm_text") == 0);
	assert(strstr(out.payload, "\"text\":\"reply\"") != NULL);
}

static void test_tts_stop_maps_to_tts_state_stop(void)
{
	xiaozhi_incoming_event_t in = {0};
	control_event_t out = {0};

	in.type = XIAOZHI_EVENT_TTS_STOP;

	assert(control_events_from_protocol(&in, &out) == 0);
	assert(strcmp(out.name, "tts_state") == 0);
	assert(strstr(out.payload, "\"state\":\"stop\"") != NULL);
}

static void test_tts_sentence_is_unsupported(void)
{
	xiaozhi_incoming_event_t in = {0};
	control_event_t out = {0};

	in.type = XIAOZHI_EVENT_TTS_SENTENCE;

	assert(control_events_from_protocol(&in, &out) != 0);
}

static void test_null_inputs_fail(void)
{
	control_event_t out = {0};
	xiaozhi_incoming_event_t in = {0};

	assert(control_events_from_protocol(NULL, &out) != 0);
	assert(control_events_from_protocol(&in, NULL) != 0);
}

static void test_playing_state_name(void)
{
	assert(strcmp(control_events_state_name(SESSION_STATE_PLAYING_TTS), "playing") == 0);
}

int main(void)
{
	test_playing_state_name();
	test_hello_maps_to_session_hello();
	test_stt_maps_to_stt_result();
	test_llm_maps_to_llm_text();
	test_tts_stop_maps_to_tts_state_stop();
	test_tts_sentence_is_unsupported();
	test_null_inputs_fail();
	return 0;
}
