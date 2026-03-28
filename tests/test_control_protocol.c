#include <assert.h>
#include <string.h>

#include "control_plane/control_protocol.h"

int main(void)
{
	control_command_t cmd = {0};
	control_event_t event = {0};
	char json[256];
	char out[256];

	strcpy(json, "{\"type\":\"command\",\"name\":\"connect_server\",\"request_id\":\"req-1\",\"payload\":{}}");
	assert(control_protocol_parse_command(json, &cmd) == 0);
	assert(strcmp(cmd.type, "command") == 0);
	assert(strcmp(cmd.name, "connect_server") == 0);
	assert(strcmp(cmd.request_id, "req-1") == 0);
	assert(strcmp(cmd.payload, "{}") == 0);

	strcpy(json, "{\"type\":\"command\",\"name\":\"connect_server\"}garbage");
	assert(control_protocol_parse_command(json, &cmd) != 0);

	strcpy(json, "{\"type\":\"command\",\"name\":\"connect_server\",\"request_id\":1}");
	assert(control_protocol_parse_command(json, &cmd) != 0);

	strcpy(json, "{\"type\":\"command\",\"name\":\"connect_server\",\"payload\":\"abc\"}");
	assert(control_protocol_parse_command(json, &cmd) == 0);
	assert(strcmp(cmd.payload, "\"abc\"") == 0);

	strcpy(event.type, "event");
	strcpy(event.name, "state_changed");
	strcpy(event.request_id, "req-2");
	event.ok = 1;
	strcpy(event.message, "ready");
	strcpy(event.payload, "{\"state\":\"ready\"}");
	assert(control_protocol_build_event(&event, out, sizeof(out)) == 0);
	assert(strstr(out, "\"type\":\"event\"") != NULL);
	assert(strstr(out, "\"name\":\"state_changed\"") != NULL);
	assert(strstr(out, "\"request_id\":\"req-2\"") != NULL);
	assert(strstr(out, "\"ok\":true") != NULL);
	assert(strstr(out, "\"message\":\"ready\"") != NULL);
	assert(strstr(out, "\"state\":\"ready\"") != NULL);

	event.name[0] = '\0';
	assert(control_protocol_build_event(&event, out, sizeof(out)) != 0);

	return 0;
}
