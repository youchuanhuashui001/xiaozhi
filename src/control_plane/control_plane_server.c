#include "control_plane/control_plane_server.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "control_plane/control_events.h"
#include "log.h"

enum {
	CONTROL_PLANE_WS_RX_BUFFER = 2048
};

typedef struct {
	unsigned long delivered_seq;
} control_plane_session_t;

static int control_plane_server_callback(struct lws *wsi,
					 enum lws_callback_reasons reason,
					 void *user, void *in, size_t len);
static void *control_plane_server_thread_main(void *arg);

static void control_plane_server_notify_clients(control_plane_server_t *server)
{
	if (!server || !server->context || !server->started)
		return;

	lws_callback_on_writable_all_protocol(server->context,
					      &server->protocols[0]);
	lws_cancel_service(server->context);
}

static int control_plane_server_publish_event(control_plane_server_t *server,
					      const control_event_t *event)
{
	int rc;

	if (!server || !event)
		return -1;

	pthread_mutex_lock(&server->event_lock);
	rc = control_protocol_build_event(event, server->last_outbound_json,
					  sizeof(server->last_outbound_json));
	if (rc == 0)
		server->outbound_seq++;
	pthread_mutex_unlock(&server->event_lock);
	if (rc != 0)
		return -1;

	control_plane_server_notify_clients(server);
	return 0;
}

static int control_plane_server_publish_error(control_plane_server_t *server,
					      const char *message)
{
	control_event_t event;

	memset(&event, 0, sizeof(event));
	if (snprintf(event.type, sizeof(event.type), "%s", "error") >=
	    (int)sizeof(event.type))
		return -1;
	if (snprintf(event.name, sizeof(event.name), "%s", "command_error") >=
	    (int)sizeof(event.name))
		return -1;
	event.ok = 0;
	if (message &&
	    snprintf(event.message, sizeof(event.message), "%s", message) >=
		    (int)sizeof(event.message))
		return -1;

	return control_plane_server_publish_event(server, &event);
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

static int control_plane_server_publish_runtime_updates(control_plane_server_t *server)
{
	control_event_t event;
	unsigned long observed_seq = 0;

	if (!server || !server->runtime)
		return -1;

	memset(&event, 0, sizeof(event));
	if (daemon_runtime_snapshot(server->runtime, &event, &observed_seq) != 0)
		return -1;

	if (observed_seq == 0 || observed_seq == server->last_runtime_seq)
		return 0;
	server->last_runtime_seq = observed_seq;

	if (event.name[0] == '\0')
		return 0;

	return control_plane_server_publish_event(server, &event);
}

int control_plane_server_init(control_plane_server_t *server,
				 daemon_runtime_t *runtime)
{
	if (!server)
		return -1;

	memset(server, 0, sizeof(*server));
	server->runtime = runtime;
	if (pthread_mutex_init(&server->event_lock, NULL) != 0)
		return -1;

	memset(server->protocols, 0, sizeof(server->protocols));
	server->protocols[0].name = "xiaozhi-control-v1";
	server->protocols[0].callback = control_plane_server_callback;
	server->protocols[0].per_session_data_size =
		sizeof(control_plane_session_t);
	server->protocols[0].rx_buffer_size = CONTROL_PLANE_WS_RX_BUFFER;
	return 0;
}

int control_plane_server_start(control_plane_server_t *server)
{
	struct lws_context_creation_info info;
	const char *bind_host = "127.0.0.1";
	int port = 19090;

	if (!server)
		return -1;
	if (!server->runtime)
		return -1;
	if (server->started)
		return 0;

	if (server->runtime->app) {
		if (server->runtime->app->config.control_plane.bind_host[0] != '\0')
			bind_host =
				server->runtime->app->config.control_plane.bind_host;
		if (server->runtime->app->config.control_plane.port > 0)
			port = server->runtime->app->config.control_plane.port;
	}

	memset(&info, 0, sizeof(info));
	info.port = port;
	info.iface = bind_host;
	info.protocols = server->protocols;
	info.user = server;
	info.gid = (gid_t)-1;
	info.uid = (uid_t)-1;

	server->context = lws_create_context(&info);
	if (!server->context)
		return -1;

	server->started = 1;
	server->stop_requested = 0;
	server->last_runtime_seq = 0;
	if (pthread_create(&server->thread, NULL, control_plane_server_thread_main,
			   server) != 0) {
		server->started = 0;
		lws_context_destroy(server->context);
		server->context = NULL;
		return -1;
	}
	log_info("control plane websocket listening on ws://%s:%d", bind_host, port);

	return 0;
}

void control_plane_server_stop(control_plane_server_t *server)
{
	if (!server)
		return;

	if (server->started) {
		server->stop_requested = 1;
		if (server->context)
			lws_cancel_service(server->context);
		pthread_join(server->thread, NULL);
	}

	if (server->context) {
		lws_context_destroy(server->context);
		server->context = NULL;
	}

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
	if (control_protocol_parse_command(json, &cmd) != 0) {
		(void)control_plane_server_publish_error(server, "invalid command json");
		return -1;
	}

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

static int control_plane_server_callback(struct lws *wsi,
					 enum lws_callback_reasons reason,
					 void *user, void *in, size_t len)
{
	struct lws_context *context;
	control_plane_server_t *server;
	control_plane_session_t *session = user;

	context = lws_get_context(wsi);
	server = lws_context_user(context);

	if (!server)
		return 0;

	switch (reason) {
	case LWS_CALLBACK_ESTABLISHED:
		if (session)
			session->delivered_seq = 0;
		(void)control_plane_server_publish_state_snapshot(server);
		lws_callback_on_writable(wsi);
		break;
	case LWS_CALLBACK_RECEIVE: {
		char *json;
		if (!in || len == 0)
			break;
		json = malloc(len + 1);
		if (!json)
			return -1;
		memcpy(json, in, len);
		json[len] = '\0';
		(void)control_plane_server_dispatch_json(server, json);
		free(json);
		lws_callback_on_writable(wsi);
		break;
	}
	case LWS_CALLBACK_SERVER_WRITEABLE: {
		char outbound[sizeof(server->last_outbound_json)];
		unsigned long send_seq = 0;
		size_t msg_len;
		unsigned char frame[LWS_PRE + sizeof(outbound)];

		if (!session)
			break;

		outbound[0] = '\0';
		pthread_mutex_lock(&server->event_lock);
		if (session->delivered_seq < server->outbound_seq &&
		    server->last_outbound_json[0] != '\0') {
			if (snprintf(outbound, sizeof(outbound), "%s",
				     server->last_outbound_json) < (int)sizeof(outbound))
				send_seq = server->outbound_seq;
		}
		pthread_mutex_unlock(&server->event_lock);

		if (send_seq == 0)
			break;

		msg_len = strlen(outbound);
		memcpy(&frame[LWS_PRE], outbound, msg_len);
		if (lws_write(wsi, &frame[LWS_PRE], msg_len, LWS_WRITE_TEXT) < 0)
			return -1;
		session->delivered_seq = send_seq;
		break;
	}
	default:
		break;
	}

	return 0;
}

static void *control_plane_server_thread_main(void *arg)
{
	control_plane_server_t *server = arg;

	if (!server || !server->context)
		return NULL;

	while (!server->stop_requested) {
		lws_service(server->context, 50);
		(void)control_plane_server_publish_runtime_updates(server);
	}

	return NULL;
}
