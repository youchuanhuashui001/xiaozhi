#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "app.h"

static app_runtime_t *global_app;

static void sigint_handler(int sig)
{
	(void)sig;
	if (global_app)
		app_request_stop(global_app);
}

static app_options_t parse_args(int argc, char **argv)
{
	app_options_t opts = {
		.config_path = "config/xiaozhi.ini",
		.check_only = 0
	};
	int i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
			opts.config_path = argv[++i];
		} else if (strcmp(argv[i], "--check-config") == 0) {
			opts.check_only = 1;
		}
	}

	return opts;
}

int main(int argc, char **argv)
{
	app_runtime_t app;
	app_options_t opts = parse_args(argc, argv);
	int rc;

	signal(SIGINT, sigint_handler);
	global_app = &app;

	rc = app_init(&app, &opts);
	if (rc != 0)
		return 1;

	if (opts.check_only) {
		app_destroy(&app);
		return 0;
	}

	rc = app_run(&app);
	app_destroy(&app);
	return rc == 0 ? 0 : 1;
}
