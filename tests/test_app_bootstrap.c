#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app.h"

static int stub_fetch_activation_required(const app_config_t *cfg,
					  ota_response_t *response,
					  char *err,
					  size_t err_size,
					  void *ctx)
{
	(void)cfg;
	(void)err;
	(void)err_size;
	(void)ctx;

	memset(response, 0, sizeof(*response));
	response->activation_required = 1;
	snprintf(response->activation.code, sizeof(response->activation.code), "123456");
	snprintf(response->activation.message, sizeof(response->activation.message),
		 "activate me");
	return 0;
}

static int stub_fetch_ready_websocket(const app_config_t *cfg,
				      ota_response_t *response,
				      char *err,
				      size_t err_size,
				      void *ctx)
{
	(void)cfg;
	(void)err;
	(void)err_size;
	(void)ctx;

	memset(response, 0, sizeof(*response));
	snprintf(response->websocket.url, sizeof(response->websocket.url),
		 "wss://ota.example/ws");
	snprintf(response->websocket.token, sizeof(response->websocket.token),
		 "ota-token");
	return 0;
}

static void write_temp_config(char *path, size_t path_size)
{
	const char *ini =
		"[ota]\n"
		"url=https://api.tenclass.net/xiaozhi/ota/\n"
		"app_version=1.0.1\n"
		"\n"
		"[board]\n"
		"type=bread-compact-wifi\n"
		"name=bread-compact-wifi-128x64\n"
		"\n"
		"[server]\n"
		"url=wss://fallback.example/ws\n"
		"token=fallback-token\n";
	int fd;
	FILE *fp;

	snprintf(path, path_size, "/tmp/test-app-bootstrap-XXXXXX");
	fd = mkstemp(path);
	assert(fd >= 0);

	fp = fdopen(fd, "w");
	assert(fp != NULL);
	assert(fputs(ini, fp) >= 0);
	fclose(fp);
}

static void test_app_init_marks_activation_pending(void)
{
	app_runtime_t app;
	app_options_t opts = {0};
	char path[128];

	write_temp_config(path, sizeof(path));
	opts.config_path = path;
	opts.ota_fetch = stub_fetch_activation_required;

	assert(app_init(&app, &opts) == 0);
	assert(app.activation_pending == 1);
	assert(strcmp(app.ota_response.activation.code, "123456") == 0);
	assert(app.runtime_modules_initialized == 0);
	assert(app_run(&app) != 0);
	app_destroy(&app);
	unlink(path);
}

static void test_app_init_prefers_ota_websocket_settings(void)
{
	app_runtime_t app;
	app_options_t opts = {0};
	char path[128];

	write_temp_config(path, sizeof(path));
	opts.config_path = path;
	opts.skip_runtime_init = 1;
	opts.ota_fetch = stub_fetch_ready_websocket;

	assert(app_init(&app, &opts) == 0);
	assert(strcmp(app.config.server.url, "wss://ota.example/ws") == 0);
	assert(strcmp(app.config.server.token, "ota-token") == 0);
	app_destroy(&app);
	unlink(path);
}

int main(void)
{
	test_app_init_marks_activation_pending();
	test_app_init_prefers_ota_websocket_settings();
	return 0;
}
