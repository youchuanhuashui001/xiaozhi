#ifndef CONTROL_PROTOCOL_H
#define CONTROL_PROTOCOL_H

#include <stddef.h>

#define CONTROL_PROTOCOL_TYPE_MAX 16
#define CONTROL_PROTOCOL_NAME_MAX 64
#define CONTROL_PROTOCOL_REQUEST_ID_MAX 64
#define CONTROL_PROTOCOL_PAYLOAD_MAX 512
#define CONTROL_PROTOCOL_MESSAGE_MAX 256

typedef struct {
	char type[CONTROL_PROTOCOL_TYPE_MAX];
	char name[CONTROL_PROTOCOL_NAME_MAX];
	char request_id[CONTROL_PROTOCOL_REQUEST_ID_MAX];
	char payload[CONTROL_PROTOCOL_PAYLOAD_MAX];
} control_command_t;

typedef struct {
	char type[CONTROL_PROTOCOL_TYPE_MAX];
	char name[CONTROL_PROTOCOL_NAME_MAX];
	char request_id[CONTROL_PROTOCOL_REQUEST_ID_MAX];
	int ok;
	char message[CONTROL_PROTOCOL_MESSAGE_MAX];
	char payload[CONTROL_PROTOCOL_PAYLOAD_MAX];
} control_event_t;

int control_protocol_parse_command(const char *json, control_command_t *out);
int control_protocol_build_event(const control_event_t *event, char *buf, size_t n);

#endif /* CONTROL_PROTOCOL_H */
