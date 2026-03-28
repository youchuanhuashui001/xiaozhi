#include "control_plane/control_plane_server.h"

#include <stdio.h>
#include <string.h>

#include "control_plane/control_events.h"

static int control_plane_server_publish_event(control_plane_server_t *server,
					      const control_event_t *event)
{
	if (!server || !event)
		return -1;

	if (control_protocol_build_event(event, server->last_outbound_json,
					 sizeof(server->last_outbound_json)) != 0)
		return -1;

	return 0;
}

static int control_plane_server_publish_state_snapshot(control_plane_server_t *server)
{
	control_event_t event;
	session_state_t state = SESSION_STATE_IDLE;
	const char *state_name;

	if (!server || !server->runtime)
		return -1;

	memset(&event, 0, sizeof(event));
	if (snprintf(event.type, sizeof(event.type), "%s", "event") >=
	    (int)sizeof(event.type))
		return -1;
	if (snprintf(event.name, sizeof(event.name), "%s", "state_changed") >=
	    (int)sizeof(event.name))
		return -1;

	if (server->runtime->app)
		state = server->runtime->app->session.state;
	state_name = control_events_state_name(state);

	if (snprintf(event.payload, sizeof(event.payload), "{\"state\":\"%s\"}",
		     state_name) >= (int)sizeof(event.payload))
		return -1;
	event.ok = 1;
	return control_plane_server_publish_event(server, &event);
}

static int control_plane_server_publish_command_ack(control_plane_server_t *server,
						    const control_command_t *cmd,
						    int ok, const char *message)
{
	control_event_t ack;

	if (!server || !cmd)
		return -1;

	memset(&ack, 0, sizeof(ack));
	if (snprintf(ack.type, sizeof(ack.type), "%s", "command_ack") >=
	    (int)sizeof(ack.type))
		return -1;
	if (snprintf(ack.name, sizeof(ack.name), "%s", cmd->name) >=
	    (int)sizeof(ack.name))
		return -1;
	if (cmd->request_id[0] != '\0' &&
	    snprintf(ack.request_id, sizeof(ack.request_id), "%s",
		     cmd->request_id) >= (int)sizeof(ack.request_id))
		return -1;
	ack.ok = ok ? 1 : 0;
	if (message && message[0] != '\0' &&
	    snprintf(ack.message, sizeof(ack.message), "%s", message) >=
	    (int)sizeof(ack.message))
		return -1;

	return control_plane_server_publish_event(server, &ack);
}

int control_plane_server_init(control_plane_server_t *server,
				 daemon_runtime_t *runtime)
{
	if (!server)
		return -1;

	memset(server, 0, sizeof(*server));
	server->runtime = runtime;
	return 0;
}

int control_plane_server_start(control_plane_server_t *server)
{
	if (!server)
		return -1;

	server->started = 1;
	return 0;
}

void control_plane_server_stop(control_plane_server_t *server)
{
	if (!server)
		return;

	server->started = 0;
}

int control_plane_server_dispatch_json(control_plane_server_t *server,
					 const char *json)
{
	control_command_t cmd;
	int rc;

	if (!server || !server->runtime || !server->started || !json)
		return -1;

	memset(&cmd, 0, sizeof(cmd));
	if (control_protocol_parse_command(json, &cmd) != 0)
		return -1;

	if (strcmp(cmd.name, "subscribe_events") == 0)
		return control_plane_server_publish_state_snapshot(server);

	rc = daemon_runtime_submit_command(server->runtime, &cmd);
	if (control_plane_server_publish_command_ack(server, &cmd, rc == 0,
						     rc == 0 ? "applied"
							     : "unsupported command") != 0)
		return -1;
	return rc;
}

const char *control_plane_server_last_outbound_json(
	const control_plane_server_t *server)
{
	if (!server)
		return NULL;
	return server->last_outbound_json;
}
