#ifndef XIAOZHI_CLIENT_H
#define XIAOZHI_CLIENT_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

#include "audio_buffer.h"

struct lws;
struct lws_context;

typedef struct {
	char url[256];
	int protocol_version;
	char authorization[256];
	char device_id[64];
	char client_id[64];
} xiaozhi_client_config_t;

typedef struct {
	void (*on_connected)(void *ctx);
	void (*on_disconnected)(void *ctx, int code);
	void (*on_text)(void *ctx, const char *payload);
	void (*on_binary)(void *ctx, const uint8_t *data, size_t len);
	void (*on_error)(void *ctx, const char *message);
} xiaozhi_client_callbacks_t;

typedef struct {
	char **items;
	size_t count;
	size_t capacity;
} xiaozhi_text_queue_t;

typedef struct {
	xiaozhi_client_config_t config;
	xiaozhi_client_callbacks_t callbacks;
	void *callback_ctx;
	xiaozhi_text_queue_t pending_text;
	audio_buffer_t pending_binary;
	pthread_mutex_t mutex;
	pthread_t thread;
	struct lws_context *context;
	struct lws *wsi;
	int running;
	int connected;
	char recv_text[4096];
	size_t recv_text_len;
	uint8_t recv_binary[4096];
	size_t recv_binary_len;
} xiaozhi_client_t;

int xiaozhi_client_init(xiaozhi_client_t *client,
			const xiaozhi_client_config_t *config,
			const xiaozhi_client_callbacks_t *callbacks,
			void *callback_ctx);
int xiaozhi_client_start(xiaozhi_client_t *client);
void xiaozhi_client_stop(xiaozhi_client_t *client);
int xiaozhi_client_queue_text(xiaozhi_client_t *client, const char *payload);
int xiaozhi_client_queue_binary(xiaozhi_client_t *client, const uint8_t *payload,
				size_t len);
size_t xiaozhi_client_pending_text_count(const xiaozhi_client_t *client);
const char *xiaozhi_client_peek_pending_text(const xiaozhi_client_t *client, size_t index);
int xiaozhi_client_is_connected(const xiaozhi_client_t *client);
void xiaozhi_client_destroy(xiaozhi_client_t *client);

#endif /* XIAOZHI_CLIENT_H */
