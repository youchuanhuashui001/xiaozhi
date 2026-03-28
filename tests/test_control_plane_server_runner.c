#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app.h"
#include "control_plane/control_plane_server.h"
#include "daemon/daemon_runtime.h"

static volatile sig_atomic_t g_stop = 0;

static void on_sigint(int sig)
{
	(void)sig;
	g_stop = 1;
}

int main(void)
{
	app_runtime_t app = {0};
	daemon_runtime_t runtime = {0};
	control_plane_server_t server = {0};
	const char *port_env = getenv("XIAOZHI_TEST_PORT");
	int port = 19190;
	int i;

	if (port_env && port_env[0] != '\0')
		port = atoi(port_env);

	snprintf(app.config.control_plane.bind_host,
		 sizeof(app.config.control_plane.bind_host), "%s", "127.0.0.1");
	app.config.control_plane.port = port;

	if (daemon_runtime_init(&runtime, &app) != 0)
		return 1;
	if (control_plane_server_init(&server, &runtime) != 0) {
		daemon_runtime_destroy(&runtime);
		return 1;
	}
	if (control_plane_server_start(&server) != 0) {
		daemon_runtime_destroy(&runtime);
		return 1;
	}

	signal(SIGINT, on_sigint);
	signal(SIGTERM, on_sigint);
	printf("control_plane_runner_ready %d\n", port);
	fflush(stdout);

	for (i = 0; i < 300 && !g_stop; i++)
		usleep(100 * 1000);

	control_plane_server_stop(&server);
	daemon_runtime_destroy(&runtime);
	return 0;
}
