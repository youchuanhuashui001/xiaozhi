#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>

typedef struct {
	char url[256];
	char token[256];
	int protocol_version;
} server_config_t;

typedef struct {
	char url[256];
	char accept_language[32];
	char app_version[64];
	char elf_sha256[96];
} ota_config_t;

typedef struct {
	char device_id[64];
	char client_id[64];
} device_config_t;

typedef struct {
	char type[64];
	char name[64];
	char ssid[64];
	int rssi;
} board_config_t;

typedef struct {
	char capture_device[64];
	char playback_device[64];
	int input_sample_rate;
	int silence_timeout_ms;
	int silence_threshold;
	int max_utterance_ms;
} audio_config_t;

typedef struct {
	char log_level[16];
	int reconnect_backoff_ms;
} runtime_config_t;

typedef struct {
	ota_config_t ota;
	server_config_t server;
	device_config_t device;
	board_config_t board;
	audio_config_t audio;
	runtime_config_t runtime;
} app_config_t;

int config_load_file(const char *path, app_config_t *out, char *err, size_t err_size);
int config_load_from_string(const char *ini, app_config_t *out, char *err, size_t err_size);

#endif /* CONFIG_H */
