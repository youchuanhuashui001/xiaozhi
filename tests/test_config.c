#include <assert.h>
#include <string.h>

#include "config.h"

static void test_config_allows_empty_server_token(void)
{
	const char *ini =
		"[server]\n"
		"url=wss://api.tenclass.net/xiaozhi/v1/\n";
	app_config_t cfg = {0};
	char err[128];

	assert(config_load_from_string(ini, &cfg, err, sizeof(err)) == 0);
	assert(cfg.server.token[0] == '\0');
}

static void test_config_applies_defaults(void)
{
	const char *ini =
		"[server]\n"
		"url=wss://api.tenclass.net/xiaozhi/v1/\n"
		"token=test-token\n";
	app_config_t cfg = {0};
	char err[128];

	assert(config_load_from_string(ini, &cfg, err, sizeof(err)) == 0);
	assert(cfg.server.protocol_version == 1);
	assert(cfg.audio.input_sample_rate == 16000);
	assert(cfg.audio.silence_threshold == 500);
	assert(cfg.snowboy.model_path[0] == '\0');
}

int main(void)
{
	test_config_allows_empty_server_token();
	test_config_applies_defaults();
	return 0;
}
