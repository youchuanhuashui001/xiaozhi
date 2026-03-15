#ifndef SESSION_H
#define SESSION_H

#include "event_queue.h"

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

typedef struct {
	session_state_t state;
} session_t;

void session_init(session_t *session);
session_action_t session_handle_event(session_t *session, app_event_type_t event);

#endif /* SESSION_H */
