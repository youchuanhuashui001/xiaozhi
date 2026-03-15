#include <assert.h>
#include <string.h>

#include "xiaozhi_client.h"

int main(void)
{
	xiaozhi_client_t client;
	xiaozhi_client_config_t cfg = {
		.url = "wss://example.invalid/ws",
		.protocol_version = 1
	};
	xiaozhi_client_callbacks_t cb = {0};

	assert(xiaozhi_client_init(&client, &cfg, &cb, NULL) == 0);
	assert(xiaozhi_client_queue_text(&client, "{\"type\":\"hello\"}") == 0);
	assert(xiaozhi_client_queue_text(&client, "{\"type\":\"listen\"}") == 0);
	assert(xiaozhi_client_pending_text_count(&client) == 2);
	assert(strcmp(xiaozhi_client_peek_pending_text(&client, 0), "{\"type\":\"hello\"}") == 0);
	assert(strcmp(xiaozhi_client_peek_pending_text(&client, 1), "{\"type\":\"listen\"}") == 0);
	xiaozhi_client_destroy(&client);

	memset(&client, 0, sizeof(client));
	cfg.url[0] = '\0';
	assert(xiaozhi_client_init(&client, &cfg, &cb, NULL) != 0);

	return 0;
}
