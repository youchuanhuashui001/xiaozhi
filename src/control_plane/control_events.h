#ifndef CONTROL_EVENTS_H
#define CONTROL_EVENTS_H

#include "control_plane/control_protocol.h"
#include "session.h"
#include "xiaozhi_protocol.h"

const char *control_events_state_name(session_state_t state);
int control_events_from_protocol(const xiaozhi_incoming_event_t *in,
				 control_event_t *out);

#endif /* CONTROL_EVENTS_H */
