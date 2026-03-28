#include "control_plane/control_protocol.h"

#include <stdio.h>
#include <string.h>

#include "cJSON.h"

static int copy_json_string(char *dst, size_t dst_size, const cJSON *item)
{
	char *printed;

	if (!dst || dst_size == 0)
		return -1;

	dst[0] = '\0';
	if (!item)
		return 0;

	if (cJSON_IsString(item) && item->valuestring) {
		if (snprintf(dst, dst_size, "%s", item->valuestring) >= (int)dst_size)
			return -1;
		return 0;
	}

	printed = cJSON_PrintUnformatted(item);
	if (!printed)
		return -1;

	if (snprintf(dst, dst_size, "%s", printed) >= (int)dst_size) {
		cJSON_free(printed);
		return -1;
	}

	cJSON_free(printed);
	return 0;
}

static int add_string_if_present(cJSON *root, const char *key, const char *value)
{
	if (!value || value[0] == '\0')
		return 0;

	if (!cJSON_AddStringToObject(root, key, value))
		return -1;

	return 0;
}

int control_protocol_parse_command(const char *json, control_command_t *out)
{
	cJSON *root;
	cJSON *type;
	cJSON *name;
	cJSON *request_id;
	cJSON *payload;

	if (!json || !out)
		return -1;

	memset(out, 0, sizeof(*out));
	root = cJSON_Parse(json);
	if (!root)
		return -1;

	type = cJSON_GetObjectItemCaseSensitive(root, "type");
	name = cJSON_GetObjectItemCaseSensitive(root, "name");
	request_id = cJSON_GetObjectItemCaseSensitive(root, "request_id");
	payload = cJSON_GetObjectItemCaseSensitive(root, "payload");

	if (!cJSON_IsString(type) || strcmp(type->valuestring, "command") != 0 ||
	    !cJSON_IsString(name)) {
		cJSON_Delete(root);
		return -1;
	}

	if (snprintf(out->type, sizeof(out->type), "%s", "command") >= (int)sizeof(out->type) ||
	    snprintf(out->name, sizeof(out->name), "%s", name->valuestring) >= (int)sizeof(out->name)) {
		cJSON_Delete(root);
		return -1;
	}

	if (cJSON_IsString(request_id) && request_id->valuestring) {
		if (snprintf(out->request_id, sizeof(out->request_id), "%s", request_id->valuestring) >=
		    (int)sizeof(out->request_id)) {
			cJSON_Delete(root);
			return -1;
		}
	}

	if (payload && copy_json_string(out->payload, sizeof(out->payload), payload) != 0) {
		cJSON_Delete(root);
		return -1;
	}

	cJSON_Delete(root);
	return 0;
}

int control_protocol_build_event(const control_event_t *event, char *buf, size_t n)
{
	cJSON *root;
	cJSON *payload;
	char *printed;
	int rc = -1;

	if (!event || !buf || n == 0)
		return -1;

	root = cJSON_CreateObject();
	if (!root)
		return -1;

	if (add_string_if_present(root, "type", event->type[0] ? event->type : "event") != 0 ||
	    add_string_if_present(root, "name", event->name) != 0 ||
	    add_string_if_present(root, "request_id", event->request_id) != 0 ||
	    !cJSON_AddBoolToObject(root, "ok", event->ok ? 1 : 0)) {
		cJSON_Delete(root);
		return -1;
	}

	if (add_string_if_present(root, "message", event->message) != 0) {
		cJSON_Delete(root);
		return -1;
	}

	if (event->payload[0] != '\0') {
		payload = cJSON_Parse(event->payload);
		if (!payload) {
			cJSON_Delete(root);
			return -1;
		}
		cJSON_AddItemToObject(root, "payload", payload);
	}

	printed = cJSON_PrintUnformatted(root);
	if (printed) {
		if (snprintf(buf, n, "%s", printed) < (int)n)
			rc = 0;
		cJSON_free(printed);
	}

	cJSON_Delete(root);
	return rc;
}
