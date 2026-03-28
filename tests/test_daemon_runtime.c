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

	assert(daemon_runtime_init(&rt, &app) == 0);
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

	control_plane_server_stop(&server);
}

int main(void)
{
	test_app_set_observer_registers_callback();
	test_daemon_runtime_tracks_last_command();
	return 0;
}
