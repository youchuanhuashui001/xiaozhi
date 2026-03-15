#include "command.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void create_response(char *buf, size_t buf_size, const char *action, const char *status, const char *msg)
{
	cJSON *root = cJSON_CreateObject();
	cJSON_AddStringToObject(root, "type", "cmd_result");
	cJSON_AddStringToObject(root, "action", action ? action : "unknown");
	cJSON_AddStringToObject(root, "status", status);
	if (msg)
		cJSON_AddStringToObject(root, "msg", msg);

	char *json_out = cJSON_PrintUnformatted(root);
	if (json_out) {
		strncpy(buf, json_out, buf_size - 1);
		buf[buf_size - 1] = '\0';
		free(json_out);
	}
	cJSON_Delete(root);
}

int command_parse(const char *json_str, char *response_buf, size_t buf_size)
{
	cJSON *root = cJSON_Parse(json_str);
	if (!root) {
		snprintf(response_buf, buf_size, "{\"type\":\"error\",\"msg\":\"invalid json\"}");
		return -1;
	}

	cJSON *type = cJSON_GetObjectItem(root, "type");
	if (!type || !cJSON_IsString(type) || strcmp(type->valuestring, "cmd") != 0) {
		snprintf(response_buf, buf_size, "{\"type\":\"error\",\"msg\":\"missing or invalid type\"}");
		cJSON_Delete(root);
		return 0;
	}

	cJSON *action = cJSON_GetObjectItem(root, "action");
	if (!action || !cJSON_IsString(action)) {
		snprintf(response_buf, buf_size, "{\"type\":\"error\",\"msg\":\"missing action\"}");
		cJSON_Delete(root);
		return 0;
	}

	const char *action_str = action->valuestring;
	int processed = 0;

	if (strcmp(action_str, "set_led") == 0) {
		cJSON *params = cJSON_GetObjectItem(root, "params");
		if (params) {
			cJSON *id = cJSON_GetObjectItem(params, "id");
			cJSON *state = cJSON_GetObjectItem(params, "state");
			if (id && cJSON_IsNumber(id) && state && cJSON_IsString(state)) {
				printf("指令执行: 设置 LED %d 为 %s\n", id->valueint, state->valuestring);
				create_response(response_buf, buf_size, action_str, "ok", NULL);
				processed = 1;
			}
		}
		if (!processed)
			create_response(response_buf, buf_size, action_str, "error", "invalid params for set_led");
	} else if (strcmp(action_str, "get_status") == 0) {
		create_response(response_buf, buf_size, action_str, "ok", "System is running");
	} else {
		create_response(response_buf, buf_size, action_str, "error", "unknown action");
	}

	cJSON_Delete(root);
	return 0;
}
