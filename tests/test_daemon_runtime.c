#include <assert.h>
#include <string.h>

#include "app.h"
#include "control_plane/control_plane_server.h"
#include "control_plane/control_protocol.h"
#include "daemon/daemon_runtime.h"

static void test_app_set_observer_registers_callback(void)
{
	app_runtime_t app = {0};

	assert(app_set_observer(&app, NULL, &app) == 0);
	assert(app.observer_fn == NULL);
	assert(app.observer_ctx == &app);
}

static void test_daemon_runtime_tracks_last_command(void)
{
	app_runtime_t app = {0};
	daemon_runtime_t rt = {0};
	control_plane_server_t server = {0};
	control_command_t cmd = {0};
	control_event_t drained = {0};

	assert(daemon_runtime_init(&rt, &app) == 0);
	while (daemon_runtime_pop_event(&rt, &drained) == 1)
		;
	assert(control_plane_server_init(&server, &rt) == 0);
	assert(control_plane_server_start(&server) == 0);

	strcpy(cmd.type, "command");
	strcpy(cmd.name, "connect_server");
	strcpy(cmd.request_id, "req-1");
	strcpy(cmd.payload, "{}");

	assert(daemon_runtime_submit_command(&rt, &cmd) == 0);
	assert(rt.last_command == DAEMON_RUNTIME_COMMAND_CONNECT_SERVER);
	assert(strcmp(rt.last_control_command.name, "connect_server") == 0);
	assert(strcmp(rt.last_control_command.request_id, "req-1") == 0);

	assert(control_plane_server_dispatch_json(
		       &server,
		       "{\"type\":\"command\",\"name\":\"connect_server\",\"request_id\":\"req-2\",\"payload\":{}}") == 0);
	assert(rt.last_command == DAEMON_RUNTIME_COMMAND_CONNECT_SERVER);
	assert(strcmp(rt.last_control_command.request_id, "req-2") == 0);
	assert(strstr(control_plane_server_last_outbound_json(&server),
		      "\"type\":\"command_ack\"") != NULL);
	assert(strstr(control_plane_server_last_outbound_json(&server),
		      "\"name\":\"connect_server\"") != NULL);

	assert(control_plane_server_dispatch_json(
		       &server,
		       "{\"type\":\"command\",\"name\":\"subscribe_events\",\"payload\":{}}") == 0);
	assert(strstr(control_plane_server_last_outbound_json(&server),
		      "\"name\":\"state_changed\"") != NULL);
	assert(strstr(control_plane_server_last_outbound_json(&server),
		      "\"state\":\"idle\"") != NULL);

	assert(control_plane_server_dispatch_json(
		       &server,
		       "{\"type\":\"command\",\"name\":\"set_audio_config\",\"request_id\":\"req-3\",\"payload\":{\"input_gain\":75}}") == 0);
	assert(strstr(control_plane_server_last_outbound_json(&server),
		      "\"type\":\"command_ack\"") != NULL);
	assert(strstr(control_plane_server_last_outbound_json(&server),
		      "\"name\":\"set_audio_config\"") != NULL);
	assert(strstr(control_plane_server_last_outbound_json(&server),
		      "\"ok\":true") != NULL);

	control_plane_server_stop(&server);
	daemon_runtime_destroy(&rt);
}

static void test_daemon_runtime_preserves_protocol_event_order(void)
{
	app_runtime_t app = {0};
	daemon_runtime_t rt = {0};
	app_observer_event_t ev = {0};
	control_event_t out = {0};
	control_event_t drained = {0};

	assert(daemon_runtime_init(&rt, &app) == 0);
	assert(app.observer_fn != NULL);
	while (daemon_runtime_pop_event(&rt, &drained) == 1)
		;

	memset(&ev, 0, sizeof(ev));
	ev.kind = APP_OBSERVER_EVENT_PROTOCOL;
	ev.protocol.type = XIAOZHI_EVENT_STT;
	strcpy(ev.protocol.text, "hello from user");
	app.observer_fn(&ev, app.observer_ctx);

	memset(&ev, 0, sizeof(ev));
	ev.kind = APP_OBSERVER_EVENT_PROTOCOL;
	ev.protocol.type = XIAOZHI_EVENT_TTS_SENTENCE;
	strcpy(ev.protocol.text, "reply from assistant");
	app.observer_fn(&ev, app.observer_ctx);

	assert(daemon_runtime_pop_event(&rt, &out) == 1);
	assert(strcmp(out.name, "stt_result") == 0);
	assert(strstr(out.payload, "\"text\":\"hello from user\"") != NULL);

	assert(daemon_runtime_pop_event(&rt, &out) == 1);
	assert(strcmp(out.name, "llm_text") == 0);
	assert(strstr(out.payload, "\"text\":\"reply from assistant\"") != NULL);

	assert(daemon_runtime_pop_event(&rt, &out) == 0);
	daemon_runtime_destroy(&rt);
}

int main(void)
{
	test_app_set_observer_registers_callback();
	test_daemon_runtime_tracks_last_command();
	test_daemon_runtime_preserves_protocol_event_order();
	return 0;
}
