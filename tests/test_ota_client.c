#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "ota_client.h"

static void test_parse_activation_required_response(void)
{
	const char *json =
		"{"
		"\"activation\":{\"code\":\"123456\",\"message\":\"activate me\"},"
		"\"websocket\":{\"url\":\"wss://api.tenclass.net/xiaozhi/v1/\",\"token\":\"ota-token\"}"
		"}";
	ota_response_t response;
	char err[128];

	assert(ota_parse_response(json, &response, err, sizeof(err)) == 0);
	assert(response.activation_required == 1);
	assert(strcmp(response.activation.code, "123456") == 0);
	assert(strcmp(response.activation.message, "activate me") == 0);
	assert(strcmp(response.websocket.url, "wss://api.tenclass.net/xiaozhi/v1/") == 0);
	assert(strcmp(response.websocket.token, "ota-token") == 0);
}

static void test_parse_activated_response_with_websocket_settings(void)
{
	const char *json =
		"{"
		"\"websocket\":{\"url\":\"wss://api.tenclass.net/xiaozhi/v1/\",\"token\":\"ota-token\"},"
		"\"firmware\":{\"version\":\"1.0.1\",\"url\":\"\"}"
		"}";
	ota_response_t response;
	char err[128];

	assert(ota_parse_response(json, &response, err, sizeof(err)) == 0);
	assert(response.activation_required == 0);
	assert(strcmp(response.websocket.url, "wss://api.tenclass.net/xiaozhi/v1/") == 0);
	assert(strcmp(response.websocket.token, "ota-token") == 0);
}

static void test_parse_rejects_invalid_json(void)
{
	ota_response_t response;
	char err[128];

	assert(ota_parse_response("{", &response, err, sizeof(err)) != 0);
	assert(strcmp(err, "invalid ota response json") == 0);
}

static void test_build_request_json_includes_required_fields(void)
{
	app_config_t cfg = {0};
	char body[1024];

	snprintf(cfg.device.device_id, sizeof(cfg.device.device_id), "11:22:33:44:55:66");
	snprintf(cfg.device.client_id, sizeof(cfg.device.client_id),
		 "7b94d69a-9808-4c59-9c9b-704333b38aff");
	snprintf(cfg.ota.app_version, sizeof(cfg.ota.app_version), "1.0.1");
	snprintf(cfg.ota.elf_sha256, sizeof(cfg.ota.elf_sha256), "sha256");
	snprintf(cfg.board.type, sizeof(cfg.board.type), "bread-compact-wifi");
	snprintf(cfg.board.name, sizeof(cfg.board.name), "bread-compact-wifi-128x64");
	snprintf(cfg.board.ssid, sizeof(cfg.board.ssid), "home");
	cfg.board.rssi = -55;

	assert(ota_build_request_json(&cfg, body, sizeof(body)) == 0);
	assert(strstr(body, "\"application\"") != NULL);
	assert(strstr(body, "\"version\":\"1.0.1\"") != NULL);
	assert(strstr(body, "\"elf_sha256\":\"sha256\"") != NULL);
	assert(strstr(body, "\"mac_address\":\"11:22:33:44:55:66\"") != NULL);
	assert(strstr(body, "\"uuid\":\"7b94d69a-9808-4c59-9c9b-704333b38aff\"") != NULL);
	assert(strstr(body, "\"type\":\"bread-compact-wifi\"") != NULL);
	assert(strstr(body, "\"name\":\"bread-compact-wifi-128x64\"") != NULL);
}

int main(void)
{
	test_parse_activation_required_response();
	test_parse_activated_response_with_websocket_settings();
	test_parse_rejects_invalid_json();
	test_build_request_json_includes_required_fields();
	return 0;
}
