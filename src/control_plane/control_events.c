#include "control_plane/control_events.h"

#include <stdio.h>
#include <string.h>

#include "cJSON.h"

static int copy_string(char *dst, size_t dst_size, const char *value)
{
	if (!dst || dst_size == 0)
		return -1;

	dst[0] = '\0';
	if (!value || value[0] == '\0')
		return 0;

	if (snprintf(dst, dst_size, "%s", value) >= (int)dst_size)
		return -1;

	return 0;
}

static int set_payload(control_event_t *out, cJSON *root)
{
	char *printed;
	int rc = -1;

	printed = cJSON_PrintUnformatted(root);
	if (!printed)
		return -1;

	if (snprintf(out->payload, sizeof(out->payload), "%s", printed) < (int)sizeof(out->payload))
		rc = 0;

	cJSON_free(printed);
	return rc;
}

const char *control_events_state_name(session_state_t state)
{
	switch (state) {
	case SESSION_STATE_IDLE:
	case SESSION_STATE_WAKE_DETECTING:
		return "idle";
	case SESSION_STATE_CONNECTING:
		return "connecting";
	case SESSION_STATE_HANDSHAKING:
		return "handshaking";
	case SESSION_STATE_READY:
	case SESSION_STATE_WAITING_TTS:
		return "ready";
	case SESSION_STATE_UPLOADING_AUDIO:
		return "uploading";
	case SESSION_STATE_PLAYING_TTS:
		return "playing";
	case SESSION_STATE_ERROR_BACKOFF:
		return "error_backoff";
	default:
		return "unknown";
	}
}

int control_events_from_protocol(const xiaozhi_incoming_event_t *in,
				 control_event_t *out)
{
	cJSON *root;

	if (!in || !out)
		return -1;

	memset(out, 0, sizeof(*out));
	if (copy_string(out->type, sizeof(out->type), "event") != 0)
		return -1;
	out->ok = 1;

	root = cJSON_CreateObject();
	if (!root)
		return -1;

	switch (in->type) {
	case XIAOZHI_EVENT_HELLO:
		if (copy_string(out->name, sizeof(out->name), "session_hello") != 0)
			goto fail;
		if (in->session_id[0] != '\0' &&
		    !cJSON_AddStringToObject(root, "session_id", in->session_id))
			goto fail;
		if (in->output_sample_rate > 0 &&
		    !cJSON_AddNumberToObject(root, "output_sample_rate", in->output_sample_rate))
			goto fail;
		if (in->output_channels > 0 &&
		    !cJSON_AddNumberToObject(root, "output_channels", in->output_channels))
			goto fail;
		break;
	case XIAOZHI_EVENT_STT:
		if (copy_string(out->name, sizeof(out->name), "stt_result") != 0)
			goto fail;
		if (in->text[0] != '\0' && !cJSON_AddStringToObject(root, "text", in->text))
			goto fail;
		break;
	case XIAOZHI_EVENT_LLM:
		if (copy_string(out->name, sizeof(out->name), "llm_text") != 0)
			goto fail;
		if (in->text[0] != '\0' && !cJSON_AddStringToObject(root, "text", in->text))
			goto fail;
		break;
	case XIAOZHI_EVENT_TTS_START:
		if (copy_string(out->name, sizeof(out->name), "tts_state") != 0)
			goto fail;
		if (!cJSON_AddStringToObject(root, "state", "start"))
			goto fail;
		break;
	case XIAOZHI_EVENT_TTS_STOP:
		if (copy_string(out->name, sizeof(out->name), "tts_state") != 0)
			goto fail;
		if (!cJSON_AddStringToObject(root, "state", "stop"))
			goto fail;
		break;
	case XIAOZHI_EVENT_TTS_SENTENCE:
		/*
		 * Some backends stream assistant text via tts sentence events instead of
		 * llm events. Surface non-empty sentence text to GUI as llm_text so the
		 * Live Conversation panel can render assistant content consistently.
		 */
		if (in->text[0] == '\0')
			goto fail;
		if (copy_string(out->name, sizeof(out->name), "llm_text") != 0)
			goto fail;
		if (!cJSON_AddStringToObject(root, "text", in->text))
			goto fail;
		break;
	default:
		goto fail;
	}

	if (set_payload(out, root) != 0)
		goto fail;

	cJSON_Delete(root);
	return 0;

fail:
	cJSON_Delete(root);
	return -1;
}
