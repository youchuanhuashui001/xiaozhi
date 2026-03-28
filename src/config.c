#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_error(char *err, size_t err_size, const char *msg)
{
	if (!err || err_size == 0)
		return;

	snprintf(err, err_size, "%s", msg);
}

static char *trim(char *s)
{
	char *end;

	while (*s && isspace((unsigned char)*s))
		s++;

	if (*s == '\0')
		return s;

	end = s + strlen(s) - 1;
	while (end > s && isspace((unsigned char)*end))
		*end-- = '\0';

	return s;
}

static void copy_str(char *dst, size_t dst_size, const char *src)
{
	if (dst_size == 0)
		return;

	snprintf(dst, dst_size, "%s", src ? src : "");
}

static void config_apply_defaults(app_config_t *cfg)
{
	memset(cfg, 0, sizeof(*cfg));
	cfg->server.protocol_version = 1;
	cfg->board.rssi = 0;
	copy_str(cfg->audio.capture_device, sizeof(cfg->audio.capture_device), "default");
	copy_str(cfg->audio.playback_device, sizeof(cfg->audio.playback_device), "default");
	cfg->audio.input_sample_rate = 16000;
	cfg->audio.silence_timeout_ms = 1200;
	cfg->audio.silence_threshold = 500;
	cfg->audio.max_utterance_ms = 15000;
	copy_str(cfg->runtime.log_level, sizeof(cfg->runtime.log_level), "info");
	cfg->runtime.reconnect_backoff_ms = 3000;
}

static void config_assign_value(app_config_t *cfg, const char *section,
				const char *key, const char *value)
{
	if (strcmp(section, "ota") == 0) {
		if (strcmp(key, "url") == 0)
			copy_str(cfg->ota.url, sizeof(cfg->ota.url), value);
		else if (strcmp(key, "accept_language") == 0)
			copy_str(cfg->ota.accept_language, sizeof(cfg->ota.accept_language), value);
		else if (strcmp(key, "app_version") == 0)
			copy_str(cfg->ota.app_version, sizeof(cfg->ota.app_version), value);
		else if (strcmp(key, "elf_sha256") == 0)
			copy_str(cfg->ota.elf_sha256, sizeof(cfg->ota.elf_sha256), value);
	} else if (strcmp(section, "server") == 0) {
		if (strcmp(key, "url") == 0)
			copy_str(cfg->server.url, sizeof(cfg->server.url), value);
		else if (strcmp(key, "token") == 0)
			copy_str(cfg->server.token, sizeof(cfg->server.token), value);
		else if (strcmp(key, "protocol_version") == 0)
			cfg->server.protocol_version = atoi(value);
	} else if (strcmp(section, "device") == 0) {
		if (strcmp(key, "device_id") == 0)
			copy_str(cfg->device.device_id, sizeof(cfg->device.device_id), value);
		else if (strcmp(key, "client_id") == 0)
			copy_str(cfg->device.client_id, sizeof(cfg->device.client_id), value);
	} else if (strcmp(section, "board") == 0) {
		if (strcmp(key, "type") == 0)
			copy_str(cfg->board.type, sizeof(cfg->board.type), value);
		else if (strcmp(key, "name") == 0)
			copy_str(cfg->board.name, sizeof(cfg->board.name), value);
		else if (strcmp(key, "ssid") == 0)
			copy_str(cfg->board.ssid, sizeof(cfg->board.ssid), value);
		else if (strcmp(key, "rssi") == 0)
			cfg->board.rssi = atoi(value);
	} else if (strcmp(section, "audio") == 0) {
		if (strcmp(key, "capture_device") == 0)
			copy_str(cfg->audio.capture_device, sizeof(cfg->audio.capture_device), value);
		else if (strcmp(key, "playback_device") == 0)
			copy_str(cfg->audio.playback_device, sizeof(cfg->audio.playback_device), value);
		else if (strcmp(key, "input_sample_rate") == 0)
			cfg->audio.input_sample_rate = atoi(value);
		else if (strcmp(key, "silence_timeout_ms") == 0)
			cfg->audio.silence_timeout_ms = atoi(value);
		else if (strcmp(key, "silence_threshold") == 0)
			cfg->audio.silence_threshold = atoi(value);
		else if (strcmp(key, "max_utterance_ms") == 0)
			cfg->audio.max_utterance_ms = atoi(value);
	} else if (strcmp(section, "runtime") == 0) {
		if (strcmp(key, "log_level") == 0)
			copy_str(cfg->runtime.log_level, sizeof(cfg->runtime.log_level), value);
		else if (strcmp(key, "reconnect_backoff_ms") == 0)
			cfg->runtime.reconnect_backoff_ms = atoi(value);
	}
}

static int config_validate(const app_config_t *cfg, char *err, size_t err_size)
{
	if (cfg->ota.url[0] == '\0') {
		set_error(err, err_size, "missing ota.url");
		return -1;
	}

	if (cfg->ota.app_version[0] == '\0') {
		set_error(err, err_size, "missing ota.app_version");
		return -1;
	}

	if (cfg->board.type[0] == '\0') {
		set_error(err, err_size, "missing board.type");
		return -1;
	}

	if (cfg->board.name[0] == '\0') {
		set_error(err, err_size, "missing board.name");
		return -1;
	}

	if (cfg->server.protocol_version <= 0) {
		set_error(err, err_size, "invalid server.protocol_version");
		return -1;
	}

	return 0;
}

int config_load_from_string(const char *ini, app_config_t *out, char *err, size_t err_size)
{
	char *copy;
	char *line;
	char *saveptr = NULL;
	char section[32] = "";

	if (!ini || !out) {
		set_error(err, err_size, "invalid arguments");
		return -1;
	}

	config_apply_defaults(out);

	copy = strdup(ini);
	if (!copy) {
		set_error(err, err_size, "out of memory");
		return -1;
	}

	line = strtok_r(copy, "\n", &saveptr);
	while (line) {
		char *trimmed = trim(line);

		if (*trimmed == '\0' || *trimmed == '#' || *trimmed == ';') {
			line = strtok_r(NULL, "\n", &saveptr);
			continue;
		}

		if (*trimmed == '[') {
			char *end = strchr(trimmed, ']');

			if (!end) {
				free(copy);
				set_error(err, err_size, "invalid section header");
				return -1;
			}

			*end = '\0';
			copy_str(section, sizeof(section), trimmed + 1);
			line = strtok_r(NULL, "\n", &saveptr);
			continue;
		}

		{
			char *eq = strchr(trimmed, '=');
			char *key;
			char *value;

			if (!eq) {
				line = strtok_r(NULL, "\n", &saveptr);
				continue;
			}

			*eq = '\0';
			key = trim(trimmed);
			value = trim(eq + 1);
			config_assign_value(out, section, key, value);
		}

		line = strtok_r(NULL, "\n", &saveptr);
	}

	free(copy);
	return config_validate(out, err, err_size);
}

int config_load_file(const char *path, app_config_t *out, char *err, size_t err_size)
{
	FILE *fp;
	long file_size;
	size_t read_size;
	char *buf;
	int rc;

	if (!path || !out) {
		set_error(err, err_size, "invalid arguments");
		return -1;
	}

	fp = fopen(path, "rb");
	if (!fp) {
		set_error(err, err_size, "failed to open config file");
		return -1;
	}

	if (fseek(fp, 0, SEEK_END) != 0) {
		fclose(fp);
		set_error(err, err_size, "failed to seek config file");
		return -1;
	}

	file_size = ftell(fp);
	if (file_size < 0) {
		fclose(fp);
		set_error(err, err_size, "failed to determine config size");
		return -1;
	}

	if (fseek(fp, 0, SEEK_SET) != 0) {
		fclose(fp);
		set_error(err, err_size, "failed to rewind config file");
		return -1;
	}

	buf = malloc((size_t)file_size + 1);
	if (!buf) {
		fclose(fp);
		set_error(err, err_size, "out of memory");
		return -1;
	}

	read_size = fread(buf, 1, (size_t)file_size, fp);
	fclose(fp);

	if (read_size != (size_t)file_size) {
		free(buf);
		set_error(err, err_size, "failed to read config file");
		return -1;
	}

	buf[file_size] = '\0';
	rc = config_load_from_string(buf, out, err, err_size);
	free(buf);

	return rc;
}
