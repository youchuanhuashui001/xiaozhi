#ifndef WS_SERVER_H
#define WS_SERVER_H

#include <libwebsockets.h>
#include <pthread.h>
#include <stddef.h>

#define MAX_PAYLOAD_SIZE 1024

// 会话数据结构
struct per_session_data_sensor {
	char msg[MAX_PAYLOAD_SIZE];
	size_t msg_len;
};

// 全局广播缓冲区，由互斥锁保护
extern char global_broadcast_msg[MAX_PAYLOAD_SIZE];
extern size_t global_broadcast_len;
extern pthread_mutex_t global_msg_mutex;

/**
 * WebSocket 协议回调函数
 */
int ws_callback_sensor(struct lws *wsi, enum lws_callback_reasons reason,
			void *user, void *in, size_t len);

#endif /* WS_SERVER_H */
