#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <libwebsockets.h>

/**
 * HTTP 协议回调函数
 */
int callback_http(struct lws *wsi, enum lws_callback_reasons reason,
		  void *user, void *in, size_t len);

/**
 * 获取 HTTP 挂载配置
 * @return 指向 lws_http_mount 结构体的指针
 */
struct lws_http_mount* http_server_get_mount(void);

#endif /* HTTP_SERVER_H */
