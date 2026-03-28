#include "session.h"

static session_action_t session_handle_error(session_t *session)
{
	session->state = SESSION_STATE_ERROR_BACKOFF;
	return SESSION_ACTION_RETURN_TO_WAKE;
}

void session_init(session_t *session)
{
	if (!session)
		return;

	session->state = SESSION_STATE_IDLE;
}

session_action_t session_handle_event(session_t *session, app_event_type_t event)
{
	if (!session)
		return SESSION_ACTION_NONE;

	if (event == APP_EVENT_ERROR)
		return session_handle_error(session);

	switch (session->state) {
	case SESSION_STATE_IDLE:
	case SESSION_STATE_WAKE_DETECTING:
		if (event == APP_EVENT_MANUAL_START ||
		    event == APP_EVENT_WAKEWORD_DETECTED) {
			session->state = SESSION_STATE_CONNECTING;
			return SESSION_ACTION_CONNECT;
		}
		break;

	case SESSION_STATE_CONNECTING:
		if (event == APP_EVENT_WS_CONNECTED) {
			session->state = SESSION_STATE_HANDSHAKING;
			return SESSION_ACTION_SEND_HELLO;
		}
		break;

	case SESSION_STATE_HANDSHAKING:
		if (event == APP_EVENT_WS_HELLO) {
			session->state = SESSION_STATE_READY;
			return SESSION_ACTION_NONE;
		}
		break;

	case SESSION_STATE_READY:
		if (event == APP_EVENT_MANUAL_START ||
		    event == APP_EVENT_WAKEWORD_DETECTED) {
			session->state = SESSION_STATE_UPLOADING_AUDIO;
			return SESSION_ACTION_START_LISTEN;
		}
		break;

	case SESSION_STATE_UPLOADING_AUDIO:
		if (event == APP_EVENT_WS_BINARY)
			return SESSION_ACTION_NONE;
		if (event == APP_EVENT_MANUAL_STOP ||
		    event == APP_EVENT_STT_RESULT) {
			session->state = SESSION_STATE_WAITING_TTS;
			return SESSION_ACTION_STOP_LISTEN;
		}
		break;

	case SESSION_STATE_PLAYING_TTS:
		if (event == APP_EVENT_MANUAL_START ||
		    event == APP_EVENT_WAKEWORD_DETECTED) {
			session->state = SESSION_STATE_CONNECTING;
			return SESSION_ACTION_ABORT;
		}
		break;

	default:
		break;
	}

	return SESSION_ACTION_NONE;
}
