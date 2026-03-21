#include "xiaozhi_client.h"

#include <libwebsockets.h>

#include <stdlib.h>
#include <string.h>

typedef struct {
	uint8_t *data;
	size_t len;
} xiaozhi_binary_message_t;

static int xiaozhi_client_callback(struct lws *wsi,
				   enum lws_callback_reasons reason,
				   void *user, void *in, size_t len);

static struct lws_protocols xiaozhi_protocols[] = {
	{ "xiaozhi-client", xiaozhi_client_callback, 0, 64 * 1024, 0, NULL, 0 },
	{ NULL, NULL, 0, 0, 0, NULL, 0 }
};

static int ensure_text_capacity(xiaozhi_client_t *client, size_t needed)
{
	char **new_items;
	size_t new_capacity;

	if (client->pending_text.capacity >= needed)
		return 0;

	new_capacity = client->pending_text.capacity ? client->pending_text.capacity * 2 : 4;
	while (new_capacity < needed)
		new_capacity *= 2;

	new_items = realloc(client->pending_text.items, new_capacity * sizeof(*new_items));
	if (!new_items)
		return -1;

	client->pending_text.items = new_items;
	client->pending_text.capacity = new_capacity;
	return 0;
}

static void xiaozhi_client_signal_writable(xiaozhi_client_t *client)
{
	if (!client)
		return;

	if (client->wsi)
		lws_callback_on_writable(client->wsi);
	if (client->context)
		lws_cancel_service(client->context);
}

static void xiaozhi_client_notify_error(xiaozhi_client_t *client, const char *message)
{
	if (client && client->callbacks.on_error)
		client->callbacks.on_error(client->callback_ctx, message ? message : "unknown");
}

static int xiaozhi_client_pop_text_locked(xiaozhi_client_t *client, char **payload)
{
	if (client->pending_text.count == 0)
		return 0;

	*payload = client->pending_text.items[0];
	if (client->pending_text.count > 1) {
		memmove(&client->pending_text.items[0], &client->pending_text.items[1],
			(client->pending_text.count - 1) * sizeof(*client->pending_text.items));
	}
	client->pending_text.count--;
	return 1;
}

static int xiaozhi_client_pop_binary_locked(xiaozhi_client_t *client,
					    xiaozhi_binary_message_t *msg)
{
	audio_block_t block;

	if (audio_buffer_pop(&client->pending_binary, &block) != 1)
		return 0;

	msg->data = malloc(block.len);
	if (!msg->data)
		return -1;

	memcpy(msg->data, block.data, block.len);
	msg->len = block.len;
	return 1;
}

int xiaozhi_client_feed_binary_fragment(xiaozhi_client_t *client,
					const uint8_t *payload,
					size_t len,
					int is_final_fragment)
{
	if (!client || !payload || len == 0)
		return -1;

	if (client->recv_binary_len + len > sizeof(client->recv_binary)) {
		client->recv_binary_len = 0;
		return -1;
	}

	memcpy(client->recv_binary + client->recv_binary_len, payload, len);
	client->recv_binary_len += len;

	if (!is_final_fragment)
		return 0;

	if (client->callbacks.on_binary) {
		client->callbacks.on_binary(client->callback_ctx,
					    client->recv_binary,
					    client->recv_binary_len);
	}
	client->recv_binary_len = 0;
	return 0;
}

static int xiaozhi_client_append_header(struct lws *wsi, const char *name,
					const char *value, unsigned char **p,
					unsigned char *end)
{
	if (!value || value[0] == '\0')
		return 0;

	return lws_add_http_header_by_name(wsi, (const unsigned char *)name,
					   (const unsigned char *)value,
					   (int)strlen(value), p, end);
}

static int xiaozhi_client_callback(struct lws *wsi,
				   enum lws_callback_reasons reason,
				   void *user, void *in, size_t len)
{
	xiaozhi_client_t *client = (xiaozhi_client_t *)lws_context_user(lws_get_context(wsi));
	(void)user;

	if (!client)
		return 0;

	switch (reason) {
	case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER: {
		unsigned char **p = (unsigned char **)in;
		unsigned char *end = *p + len;
		char protocol_version[16];

		snprintf(protocol_version, sizeof(protocol_version), "%d",
			 client->config.protocol_version);
		if (xiaozhi_client_append_header(wsi, "Authorization:",
						 client->config.authorization, p, end))
			return -1;
		if (xiaozhi_client_append_header(wsi, "Protocol-Version:",
						 protocol_version, p, end))
			return -1;
		if (xiaozhi_client_append_header(wsi, "Device-Id:",
						 client->config.device_id, p, end))
			return -1;
		if (xiaozhi_client_append_header(wsi, "Client-Id:",
						 client->config.client_id, p, end))
			return -1;
		break;
	}

	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		pthread_mutex_lock(&client->mutex);
		client->wsi = wsi;
		client->connected = 1;
		pthread_mutex_unlock(&client->mutex);
		if (client->callbacks.on_connected)
			client->callbacks.on_connected(client->callback_ctx);
		xiaozhi_client_signal_writable(client);
		break;

	case LWS_CALLBACK_CLIENT_RECEIVE:
		if (lws_frame_is_binary(wsi)) {
			int is_final_fragment =
				(lws_remaining_packet_payload(wsi) == 0 &&
				 lws_is_final_fragment(wsi));

			if (xiaozhi_client_feed_binary_fragment(client,
							(const uint8_t *)in,
							len,
							is_final_fragment) != 0) {
				xiaozhi_client_notify_error(client,
							    "received binary frame too large");
			}
			break;
		}

		if (client->recv_text_len + len >= sizeof(client->recv_text)) {
			client->recv_text_len = 0;
			xiaozhi_client_notify_error(client, "received text frame too large");
			break;
		}

		memcpy(client->recv_text + client->recv_text_len, in, len);
		client->recv_text_len += len;

		if (lws_remaining_packet_payload(wsi) == 0 && lws_is_final_fragment(wsi)) {
			client->recv_text[client->recv_text_len] = '\0';
			if (client->callbacks.on_text)
				client->callbacks.on_text(client->callback_ctx,
							  client->recv_text);
			client->recv_text_len = 0;
		}
		break;

	case LWS_CALLBACK_CLIENT_WRITEABLE: {
		char *text_payload = NULL;
		xiaozhi_binary_message_t binary_payload = {0};
		int pop_rc;

		pthread_mutex_lock(&client->mutex);
		pop_rc = xiaozhi_client_pop_text_locked(client, &text_payload);
		if (pop_rc == 0)
			pop_rc = xiaozhi_client_pop_binary_locked(client, &binary_payload);
		pthread_mutex_unlock(&client->mutex);

		if (pop_rc == 1 && text_payload) {
			size_t payload_len = strlen(text_payload);
			unsigned char *buf = malloc(LWS_PRE + payload_len);

			if (!buf) {
				free(text_payload);
				return -1;
			}

			memcpy(buf + LWS_PRE, text_payload, payload_len);
			if (lws_write(wsi, buf + LWS_PRE, payload_len, LWS_WRITE_TEXT) < 0) {
				free(buf);
				free(text_payload);
				return -1;
			}
			free(buf);
			free(text_payload);
		} else if (pop_rc == 1 && binary_payload.data) {
			unsigned char *buf = malloc(LWS_PRE + binary_payload.len);

			if (!buf) {
				free(binary_payload.data);
				return -1;
			}

			memcpy(buf + LWS_PRE, binary_payload.data, binary_payload.len);
			if (lws_write(wsi, buf + LWS_PRE, binary_payload.len,
				      LWS_WRITE_BINARY) < 0) {
				free(buf);
				free(binary_payload.data);
				return -1;
			}
			free(buf);
			free(binary_payload.data);
		} else if (pop_rc < 0) {
			return -1;
		}

		pthread_mutex_lock(&client->mutex);
		if (client->pending_text.count > 0 ||
		    audio_buffer_size(&client->pending_binary) > 0) {
			pthread_mutex_unlock(&client->mutex);
			lws_callback_on_writable(wsi);
		} else {
			pthread_mutex_unlock(&client->mutex);
		}
		break;
	}

	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		pthread_mutex_lock(&client->mutex);
		client->connected = 0;
		client->wsi = NULL;
		pthread_mutex_unlock(&client->mutex);
		xiaozhi_client_notify_error(client, in ? (const char *)in :
					      "client connection error");
		break;

	case LWS_CALLBACK_CLIENT_CLOSED:
		pthread_mutex_lock(&client->mutex);
		client->connected = 0;
		client->wsi = NULL;
		pthread_mutex_unlock(&client->mutex);
		if (client->callbacks.on_disconnected)
			client->callbacks.on_disconnected(client->callback_ctx, 0);
		break;

	default:
		break;
	}

	return 0;
}

static void *xiaozhi_client_thread(void *arg)
{
	xiaozhi_client_t *client = arg;
	struct lws_context_creation_info info;
	struct lws_client_connect_info ccinfo;
	char url_copy[sizeof(client->config.url)];
	char full_path[256];
	const char *prot = NULL;
	const char *address = NULL;
	const char *path = NULL;
	int port = 0;
	int ssl_flags = 0;

	memset(&info, 0, sizeof(info));
	info.port = CONTEXT_PORT_NO_LISTEN;
	info.protocols = xiaozhi_protocols;
	info.gid = -1;
	info.uid = -1;
	info.user = client;
	info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

	client->context = lws_create_context(&info);
	if (!client->context) {
		xiaozhi_client_notify_error(client, "failed to create lws context");
		return NULL;
	}

	snprintf(url_copy, sizeof(url_copy), "%s", client->config.url);
	if (lws_parse_uri(url_copy, &prot, &address, &port, &path) != 0) {
		xiaozhi_client_notify_error(client, "failed to parse websocket url");
		lws_context_destroy(client->context);
		client->context = NULL;
		return NULL;
	}

	if (prot && strcmp(prot, "wss") == 0)
		ssl_flags = LCCSCF_USE_SSL;

	memset(&ccinfo, 0, sizeof(ccinfo));
	ccinfo.context = client->context;
	ccinfo.address = address;
	ccinfo.port = port;
	snprintf(full_path, sizeof(full_path), "/%s", path ? path : "");
	ccinfo.path = full_path;
	ccinfo.host = address;
	ccinfo.origin = address;
	ccinfo.protocol = xiaozhi_protocols[0].name;
	ccinfo.local_protocol_name = xiaozhi_protocols[0].name;
	ccinfo.ssl_connection = ssl_flags;
	ccinfo.pwsi = &client->wsi;

	if (!lws_client_connect_via_info(&ccinfo)) {
		xiaozhi_client_notify_error(client, "failed to initiate websocket connection");
		lws_context_destroy(client->context);
		client->context = NULL;
		return NULL;
	}

	while (client->running)
		lws_service(client->context, 50);

	lws_context_destroy(client->context);
	client->context = NULL;
	client->wsi = NULL;
	client->connected = 0;
	return NULL;
}

int xiaozhi_client_init(xiaozhi_client_t *client,
			const xiaozhi_client_config_t *config,
			const xiaozhi_client_callbacks_t *callbacks,
			void *callback_ctx)
{
	if (!client || !config || config->url[0] == '\0')
		return -1;

	memset(client, 0, sizeof(*client));
	client->config = *config;
	if (callbacks)
		client->callbacks = *callbacks;
	client->callback_ctx = callback_ctx;
	if (audio_buffer_init(&client->pending_binary, 32, 4096) != 0)
		return -1;
	pthread_mutex_init(&client->mutex, NULL);

	return 0;
}

int xiaozhi_client_start(xiaozhi_client_t *client)
{
	if (!client)
		return -1;

	pthread_mutex_lock(&client->mutex);
	if (client->running) {
		pthread_mutex_unlock(&client->mutex);
		return 0;
	}
	client->running = 1;
	pthread_mutex_unlock(&client->mutex);

	if (pthread_create(&client->thread, NULL, xiaozhi_client_thread, client) != 0) {
		pthread_mutex_lock(&client->mutex);
		client->running = 0;
		pthread_mutex_unlock(&client->mutex);
		return -1;
	}

	return 0;
}

void xiaozhi_client_stop(xiaozhi_client_t *client)
{
	if (!client)
		return;

	pthread_mutex_lock(&client->mutex);
	if (!client->running) {
		pthread_mutex_unlock(&client->mutex);
		return;
	}
	client->running = 0;
	pthread_mutex_unlock(&client->mutex);

	if (client->context)
		lws_cancel_service(client->context);

	if (client->thread)
		pthread_join(client->thread, NULL);
	client->thread = 0;
}

int xiaozhi_client_queue_text(xiaozhi_client_t *client, const char *payload)
{
	char *copy;
	int rc;

	if (!client || !payload)
		return -1;

	pthread_mutex_lock(&client->mutex);
	rc = ensure_text_capacity(client, client->pending_text.count + 1);
	pthread_mutex_unlock(&client->mutex);
	if (rc != 0)
		return -1;

	copy = strdup(payload);
	if (!copy)
		return -1;

	pthread_mutex_lock(&client->mutex);
	client->pending_text.items[client->pending_text.count++] = copy;
	pthread_mutex_unlock(&client->mutex);

	xiaozhi_client_signal_writable(client);
	return 0;
}

int xiaozhi_client_queue_binary(xiaozhi_client_t *client, const uint8_t *payload,
				size_t len)
{
	int rc;

	if (!client || !payload || len == 0)
		return -1;

	pthread_mutex_lock(&client->mutex);
	rc = audio_buffer_push(&client->pending_binary, payload, len, 0);
	pthread_mutex_unlock(&client->mutex);
	if (rc == 0)
		xiaozhi_client_signal_writable(client);

	return rc;
}

size_t xiaozhi_client_pending_text_count(const xiaozhi_client_t *client)
{
	if (!client)
		return 0;

	return client->pending_text.count;
}

const char *xiaozhi_client_peek_pending_text(const xiaozhi_client_t *client, size_t index)
{
	if (!client || index >= client->pending_text.count)
		return NULL;

	return client->pending_text.items[index];
}

int xiaozhi_client_is_connected(const xiaozhi_client_t *client)
{
	if (!client)
		return 0;

	return client->connected;
}

void xiaozhi_client_destroy(xiaozhi_client_t *client)
{
	size_t i;

	if (!client)
		return;

	xiaozhi_client_stop(client);

	for (i = 0; i < client->pending_text.count; i++)
		free(client->pending_text.items[i]);
	free(client->pending_text.items);
	audio_buffer_destroy(&client->pending_binary);
	pthread_mutex_destroy(&client->mutex);
	memset(client, 0, sizeof(*client));
}
