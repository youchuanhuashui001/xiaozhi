#include <signal.h>
#include <string.h>

#include "app.h"
#include "control_plane/control_plane_server.h"
#include "daemon/daemon_runtime.h"

static app_runtime_t *g_app = NULL;

static void daemon_sigint_handler(int sig)
{
	(void)sig;
	if (g_app)
		app_request_stop(g_app);
}

static app_options_t daemon_parse_args(int argc, char **argv)
{
	app_options_t opts = {
		.config_path = "config/xiaozhi.ini",
		.check_only = 0
	};
	int i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--config") == 0 && i + 1 < argc)
			opts.config_path = argv[++i];
		else if (strcmp(argv[i], "--check-config") == 0)
			opts.check_only = 1;
	}

	return opts;
}

int main(int argc, char **argv)
{
	app_runtime_t app;
	daemon_runtime_t runtime;
	control_plane_server_t server;
	app_options_t opts = daemon_parse_args(argc, argv);
	int rc;

	memset(&app, 0, sizeof(app));
	memset(&runtime, 0, sizeof(runtime));
	memset(&server, 0, sizeof(server));

	signal(SIGINT, daemon_sigint_handler);
	g_app = &app;

	rc = app_init(&app, &opts);
	if (rc != 0)
		return 1;

	if (opts.check_only) {
		app_destroy(&app);
		return 0;
	}

	if (daemon_runtime_init(&runtime, &app) != 0) {
		app_destroy(&app);
		return 1;
	}

	if (control_plane_server_init(&server, &runtime) != 0 ||
	    control_plane_server_start(&server) != 0) {
		daemon_runtime_destroy(&runtime);
		app_destroy(&app);
		return 1;
	}

	rc = app_run(&app);
	control_plane_server_stop(&server);
	daemon_runtime_destroy(&runtime);
	app_destroy(&app);
	return rc == 0 ? 0 : 1;
}
