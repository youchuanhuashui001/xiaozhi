#include <assert.h>

#include "session.h"

static void test_happy_path(void)
{
	session_t s;

	session_init(&s);
	assert(s.state == SESSION_STATE_IDLE);

	assert(session_handle_event(&s, APP_EVENT_MANUAL_START) == SESSION_ACTION_CONNECT);
	assert(s.state == SESSION_STATE_CONNECTING);

	assert(session_handle_event(&s, APP_EVENT_WS_CONNECTED) == SESSION_ACTION_SEND_HELLO);
	assert(s.state == SESSION_STATE_HANDSHAKING);

	assert(session_handle_event(&s, APP_EVENT_WS_HELLO) == SESSION_ACTION_NONE);
	assert(s.state == SESSION_STATE_READY);

	assert(session_handle_event(&s, APP_EVENT_MANUAL_START) == SESSION_ACTION_START_LISTEN);
	assert(s.state == SESSION_STATE_UPLOADING_AUDIO);
}

static void test_unexpected_binary_while_uploading_is_ignored(void)
{
	session_t s;

	session_init(&s);
	s.state = SESSION_STATE_UPLOADING_AUDIO;
	assert(session_handle_event(&s, APP_EVENT_WS_BINARY) == SESSION_ACTION_NONE);
	assert(s.state == SESSION_STATE_UPLOADING_AUDIO);
}

static void test_error_moves_to_backoff(void)
{
	session_t s;

	session_init(&s);
	s.state = SESSION_STATE_READY;
	assert(session_handle_event(&s, APP_EVENT_ERROR) == SESSION_ACTION_RETURN_TO_WAKE);
	assert(s.state == SESSION_STATE_ERROR_BACKOFF);
}

static void test_silence_timeout_is_ignored_while_uploading(void)
{
	session_t s;

	session_init(&s);
	s.state = SESSION_STATE_UPLOADING_AUDIO;
	assert(session_handle_event(&s, APP_EVENT_CAPTURE_SILENCE_TIMEOUT) == SESSION_ACTION_NONE);
	assert(s.state == SESSION_STATE_UPLOADING_AUDIO);
}

static void test_manual_stop_stops_listen(void)
{
	session_t s;

	session_init(&s);
	s.state = SESSION_STATE_UPLOADING_AUDIO;
	assert(session_handle_event(&s, APP_EVENT_MANUAL_STOP) == SESSION_ACTION_STOP_LISTEN);
	assert(s.state == SESSION_STATE_WAITING_TTS);
}

static void test_silence_timeout_is_ignored_when_not_uploading(void)
{
	session_t s;

	session_init(&s);
	s.state = SESSION_STATE_READY;
	assert(session_handle_event(&s, APP_EVENT_CAPTURE_SILENCE_TIMEOUT) == SESSION_ACTION_NONE);
	assert(s.state == SESSION_STATE_READY);
}

static void test_barge_in_requests_abort(void)
{
	session_t s;

	session_init(&s);
	s.state = SESSION_STATE_PLAYING_TTS;
	assert(session_handle_event(&s, APP_EVENT_MANUAL_START) == SESSION_ACTION_ABORT);
	assert(s.state == SESSION_STATE_CONNECTING);
}

int main(void)
{
	test_happy_path();
	test_unexpected_binary_while_uploading_is_ignored();
	test_error_moves_to_backoff();
	test_silence_timeout_is_ignored_while_uploading();
	test_manual_stop_stops_listen();
	test_silence_timeout_is_ignored_when_not_uploading();
	test_barge_in_requests_abort();
	return 0;
}
