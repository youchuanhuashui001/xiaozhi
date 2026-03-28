#include <assert.h>
#include <string.h>

#include "config.h"

static void test_config_allows_empty_server_token(void)
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
		"url=wss://api.tenclass.net/xiaozhi/v1/\n";
	app_config_t cfg = {0};
	char err[128];

	assert(config_load_from_string(ini, &cfg, err, sizeof(err)) == 0);
	assert(cfg.server.token[0] == '\0');
	assert(strcmp(cfg.ota.url, "https://api.tenclass.net/xiaozhi/ota/") == 0);
	assert(strcmp(cfg.board.type, "bread-compact-wifi") == 0);
	assert(strcmp(cfg.board.name, "bread-compact-wifi-128x64") == 0);
}

static void test_config_applies_defaults(void)
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
		"url=wss://api.tenclass.net/xiaozhi/v1/\n"
		"token=test-token\n";
	app_config_t cfg = {0};
	char err[128];

	assert(config_load_from_string(ini, &cfg, err, sizeof(err)) == 0);
	assert(cfg.server.protocol_version == 1);
	assert(cfg.audio.input_sample_rate == 16000);
	assert(cfg.audio.silence_threshold == 500);
	assert(strcmp(cfg.ota.accept_language, "") == 0);
	assert(strcmp(cfg.ota.app_version, "1.0.1") == 0);
	assert(strcmp(cfg.runtime.dialog_mode, "manual") == 0);
}

static void test_config_parses_auto_dialog_mode(void)
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
		"[runtime]\n"
		"dialog_mode=auto\n";
	app_config_t cfg = {0};
	char err[128];

	assert(config_load_from_string(ini, &cfg, err, sizeof(err)) == 0);
	assert(strcmp(cfg.runtime.dialog_mode, "auto") == 0);
}

static void test_config_requires_ota_url(void)
{
	const char *ini =
		"[ota]\n"
		"app_version=1.0.1\n"
		"\n"
		"[board]\n"
		"type=bread-compact-wifi\n"
		"name=bread-compact-wifi-128x64\n"
		"\n"
		"[server]\n"
		"url=wss://api.tenclass.net/xiaozhi/v1/\n";
	app_config_t cfg = {0};
	char err[128];

	assert(config_load_from_string(ini, &cfg, err, sizeof(err)) != 0);
	assert(strcmp(err, "missing ota.url") == 0);
}

static void test_config_requires_ota_app_version(void)
{
	const char *ini =
		"[ota]\n"
		"url=https://api.tenclass.net/xiaozhi/ota/\n"
		"\n"
		"[board]\n"
		"type=bread-compact-wifi\n"
		"name=bread-compact-wifi-128x64\n";
	app_config_t cfg = {0};
	char err[128];

	assert(config_load_from_string(ini, &cfg, err, sizeof(err)) != 0);
	assert(strcmp(err, "missing ota.app_version") == 0);
}

int main(void)
{
	test_config_allows_empty_server_token();
	test_config_applies_defaults();
	test_config_parses_auto_dialog_mode();
	test_config_requires_ota_url();
	test_config_requires_ota_app_version();
	return 0;
}
