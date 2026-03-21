#include "ota_client.h"

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "cJSON.h"

static void ota_set_error(char *err, size_t err_size, const char *message)
{
	if (!err || err_size == 0)
		return;

	snprintf(err, err_size, "%s", message ? message : "unknown ota error");
}

static void ota_copy_json_string(cJSON *object, const char *key,
				 char *dst, size_t dst_size)
{
	cJSON *item;

	if (!object || !key || !dst || dst_size == 0)
		return;

	item = cJSON_GetObjectItemCaseSensitive(object, key);
	if (!cJSON_IsString(item) || !item->valuestring)
		return;

	snprintf(dst, dst_size, "%s", item->valuestring);
}

static void ota_trim_trailing_whitespace(char *text)
{
	size_t len;

	if (!text)
		return;

	len = strlen(text);
	while (len > 0) {
		unsigned char ch = (unsigned char)text[len - 1];

		if (ch != ' ' && ch != '\n' && ch != '\r' && ch != '\t')
			break;
		text[--len] = '\0';
	}
}

static int ota_append_pipe_output(int fd, char *dst, size_t dst_size)
{
	char buffer[256];
	size_t used = 0;
	ssize_t nread;

	if (!dst || dst_size == 0)
		return -1;

	dst[0] = '\0';
	while ((nread = read(fd, buffer, sizeof(buffer))) > 0) {
		size_t to_copy;

		if (used >= dst_size - 1)
			continue;

		to_copy = (size_t)nread;
		if (to_copy > dst_size - 1 - used)
			to_copy = dst_size - 1 - used;
		memcpy(dst + used, buffer, to_copy);
		used += to_copy;
		dst[used] = '\0';
	}

	return nread < 0 ? -1 : 0;
}

static void ota_add_optional_string(cJSON *object, const char *key, const char *value)
{
	if (!object || !key || !value || value[0] == '\0')
		return;

	cJSON_AddStringToObject(object, key, value);
}

int ota_build_request_json(const app_config_t *cfg, char *out, size_t out_size)
{
	cJSON *root;
	cJSON *application;
	cJSON *board;
	char *json;

	if (!cfg || !out || out_size == 0)
		return -1;

	root = cJSON_CreateObject();
	if (!root)
		return -1;

	application = cJSON_AddObjectToObject(root, "application");
	board = cJSON_AddObjectToObject(root, "board");
	if (!application || !board) {
		cJSON_Delete(root);
		return -1;
	}

	cJSON_AddStringToObject(application, "version", cfg->ota.app_version);
	ota_add_optional_string(application, "elf_sha256", cfg->ota.elf_sha256);
	ota_add_optional_string(root, "mac_address", cfg->device.device_id);
	ota_add_optional_string(root, "uuid", cfg->device.client_id);
	cJSON_AddStringToObject(board, "type", cfg->board.type);
	cJSON_AddStringToObject(board, "name", cfg->board.name);
	ota_add_optional_string(board, "ssid", cfg->board.ssid);
	if (cfg->board.rssi != 0)
		cJSON_AddNumberToObject(board, "rssi", cfg->board.rssi);

	json = cJSON_PrintUnformatted(root);
	cJSON_Delete(root);
	if (!json)
		return -1;

	if (snprintf(out, out_size, "%s", json) >= (int)out_size) {
		free(json);
		return -1;
	}

	free(json);
	return 0;
}

int ota_fetch(const app_config_t *cfg,
	      ota_response_t *out,
	      char *err,
	      size_t err_size,
	      void *ctx)
{
	char body[2048];
	char response[8192];
	char header_device[128];
	char header_client[128];
	char header_user_agent[160];
	char header_language[96];
	int pipefd[2];
	pid_t pid;
	int status;
	(void)ctx;

	if (!cfg || !out) {
		ota_set_error(err, err_size, "invalid arguments");
		return -1;
	}

	if (ota_build_request_json(cfg, body, sizeof(body)) != 0) {
		ota_set_error(err, err_size, "failed to build ota request body");
		return -1;
	}

	snprintf(header_device, sizeof(header_device), "Device-Id: %s",
		 cfg->device.device_id);
	snprintf(header_client, sizeof(header_client), "Client-Id: %s",
		 cfg->device.client_id);
	snprintf(header_user_agent, sizeof(header_user_agent), "User-Agent: %s/%s",
		 cfg->board.name, cfg->ota.app_version);
	snprintf(header_language, sizeof(header_language), "Accept-Language: %s",
		 cfg->ota.accept_language);

	if (pipe(pipefd) != 0) {
		ota_set_error(err, err_size, "failed to create ota pipe");
		return -1;
	}

	pid = fork();
	if (pid < 0) {
		close(pipefd[0]);
		close(pipefd[1]);
		ota_set_error(err, err_size, "failed to fork ota curl process");
		return -1;
	}

	if (pid == 0) {
		close(pipefd[0]);
		dup2(pipefd[1], STDOUT_FILENO);
		dup2(pipefd[1], STDERR_FILENO);
		close(pipefd[1]);

		if (cfg->ota.accept_language[0] != '\0') {
			execlp("curl", "curl",
			       "-sS",
			       "-X", "POST",
			       cfg->ota.url,
			       "-H", "Content-Type: application/json",
			       "-H", header_device,
			       "-H", header_client,
			       "-H", header_user_agent,
			       "-H", header_language,
			       "--data-binary", body,
			       (char *)NULL);
		} else {
			execlp("curl", "curl",
			       "-sS",
			       "-X", "POST",
			       cfg->ota.url,
			       "-H", "Content-Type: application/json",
			       "-H", header_device,
			       "-H", header_client,
			       "-H", header_user_agent,
			       "--data-binary", body,
			       (char *)NULL);
		}

		_exit(errno == ENOENT ? 127 : 126);
	}

	close(pipefd[1]);
	if (ota_append_pipe_output(pipefd[0], response, sizeof(response)) != 0) {
		close(pipefd[0]);
		(void)waitpid(pid, &status, 0);
		ota_set_error(err, err_size, "failed to read ota response");
		return -1;
	}
	close(pipefd[0]);

	if (waitpid(pid, &status, 0) < 0) {
		ota_set_error(err, err_size, "failed to wait for ota curl process");
		return -1;
	}

	ota_trim_trailing_whitespace(response);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		if (response[0] != '\0')
			ota_set_error(err, err_size, response);
		else if (WIFEXITED(status) && WEXITSTATUS(status) == 127)
			ota_set_error(err, err_size, "curl command not found");
		else
			ota_set_error(err, err_size, "ota request failed");
		return -1;
	}

	return ota_parse_response(response, out, err, err_size);
}

int ota_parse_response(const char *json, ota_response_t *out, char *err, size_t err_size)
{
	cJSON *root;
	cJSON *activation;
	cJSON *websocket;

	if (!json || !out) {
		ota_set_error(err, err_size, "invalid arguments");
		return -1;
	}

	memset(out, 0, sizeof(*out));

	root = cJSON_Parse(json);
	if (!root) {
		ota_set_error(err, err_size, "invalid ota response json");
		return -1;
	}

	activation = cJSON_GetObjectItemCaseSensitive(root, "activation");
	if (cJSON_IsObject(activation)) {
		ota_copy_json_string(activation, "code",
				     out->activation.code,
				     sizeof(out->activation.code));
		ota_copy_json_string(activation, "message",
				     out->activation.message,
				     sizeof(out->activation.message));
		if (out->activation.code[0] != '\0')
			out->activation_required = 1;
	}

	websocket = cJSON_GetObjectItemCaseSensitive(root, "websocket");
	if (cJSON_IsObject(websocket)) {
		ota_copy_json_string(websocket, "url",
				     out->websocket.url,
				     sizeof(out->websocket.url));
		ota_copy_json_string(websocket, "token",
				     out->websocket.token,
				     sizeof(out->websocket.token));
	}

	cJSON_Delete(root);
	return 0;
}
