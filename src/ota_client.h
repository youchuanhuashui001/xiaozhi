#ifndef OTA_CLIENT_H
#define OTA_CLIENT_H

#include <stddef.h>

#include "config.h"

typedef struct {
	char code[64];
	char message[256];
} ota_activation_info_t;

typedef struct {
	char url[256];
	char token[256];
} ota_websocket_info_t;

typedef struct {
	int activation_required;
	ota_activation_info_t activation;
	ota_websocket_info_t websocket;
} ota_response_t;

typedef int (*ota_fetch_fn_t)(const app_config_t *cfg,
			      ota_response_t *out,
			      char *err,
			      size_t err_size,
			      void *ctx);

int ota_build_request_json(const app_config_t *cfg, char *out, size_t out_size);
int ota_fetch(const app_config_t *cfg,
	      ota_response_t *out,
	      char *err,
	      size_t err_size,
	      void *ctx);
int ota_parse_response(const char *json, ota_response_t *out, char *err, size_t err_size);

#endif /* OTA_CLIENT_H */
