#include <assert.h>
#include <string.h>

#include "control_plane/control_events.h"
#include "control_plane/control_protocol.h"
#include "session.h"
#include "xiaozhi_protocol.h"

int main(void)
{
	xiaozhi_incoming_event_t in = {0};
	control_event_t out = {0};

	assert(strcmp(control_events_state_name(SESSION_STATE_PLAYING_TTS), "playing") == 0);
	in.type = XIAOZHI_EVENT_TTS_START;
	assert(control_events_from_protocol(&in, &out) == 0);
	assert(strcmp(out.name, "tts_state") == 0);
	assert(strstr(out.payload, "\"state\":\"start\"") != NULL);
	return 0;
}
