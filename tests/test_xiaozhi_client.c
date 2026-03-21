#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "xiaozhi_client.h"
#include "xiaozhi_client_internal.h"

typedef struct {
	int called;
	uint8_t payload[16];
	size_t len;
} binary_capture_t;

static void on_binary_capture(void *ctx, const uint8_t *data, size_t len)
{
	binary_capture_t *capture = ctx;

	assert(capture != NULL);
	assert(data != NULL);
	assert(len <= sizeof(capture->payload));
	memcpy(capture->payload, data, len);
	capture->len = len;
	capture->called++;
}

int main(void)
{
	xiaozhi_client_t client;
	xiaozhi_client_config_t cfg = {
		.url = "wss://example.invalid/ws",
		.protocol_version = 1
	};
	xiaozhi_client_callbacks_t cb = {0};
	binary_capture_t capture = {0};
	uint8_t part1[2] = {0x11, 0x22};
	uint8_t part2[3] = {0x33, 0x44, 0x55};
	uint8_t expected[5] = {0x11, 0x22, 0x33, 0x44, 0x55};

	cb.on_binary = on_binary_capture;
	assert(xiaozhi_client_init(&client, &cfg, &cb, &capture) == 0);
	assert(xiaozhi_client_queue_text(&client, "{\"type\":\"hello\"}") == 0);
	assert(xiaozhi_client_queue_text(&client, "{\"type\":\"listen\"}") == 0);
	assert(xiaozhi_client_pending_text_count(&client) == 2);
	assert(strcmp(xiaozhi_client_peek_pending_text(&client, 0), "{\"type\":\"hello\"}") == 0);
	assert(strcmp(xiaozhi_client_peek_pending_text(&client, 1), "{\"type\":\"listen\"}") == 0);
	assert(xiaozhi_client_feed_binary_fragment(&client, part1, sizeof(part1), 0) == 0);
	assert(capture.called == 0);
	assert(xiaozhi_client_feed_binary_fragment(&client, part2, sizeof(part2), 1) == 0);
	assert(capture.called == 1);
	assert(capture.len == sizeof(expected));
	assert(memcmp(capture.payload, expected, sizeof(expected)) == 0);
	xiaozhi_client_destroy(&client);

	memset(&client, 0, sizeof(client));
	cfg.url[0] = '\0';
	assert(xiaozhi_client_init(&client, &cfg, &cb, NULL) != 0);

	return 0;
}
