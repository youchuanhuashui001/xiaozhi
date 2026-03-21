#ifndef XIAOZHI_CLIENT_INTERNAL_H
#define XIAOZHI_CLIENT_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "xiaozhi_client.h"

int xiaozhi_client_feed_binary_fragment(xiaozhi_client_t *client,
					const uint8_t *payload,
					size_t len,
					int is_final_fragment);

#endif /* XIAOZHI_CLIENT_INTERNAL_H */
