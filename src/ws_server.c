#include "ws_server.h"
#include "command.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// 定义全局广播变量
char global_broadcast_msg[MAX_PAYLOAD_SIZE];
size_t global_broadcast_len = 0;
pthread_mutex_t global_msg_mutex = PTHREAD_MUTEX_INITIALIZER;

int ws_callback_sensor(struct lws *wsi, enum lws_callback_reasons reason,
			void *user, void *in, size_t len)
{
	struct per_session_data_sensor *pss = (struct per_session_data_sensor *)user;
	char response[MAX_PAYLOAD_SIZE];

	switch (reason) {
	case LWS_CALLBACK_ESTABLISHED:
		printf("WebSocket 连接建立: %p\n", wsi);
		pss->msg_len = 0;
		break;

	case LWS_CALLBACK_RECEIVE:
		// 处理收到的指令
		if (len < MAX_PAYLOAD_SIZE) {
			char *received = malloc(len + 1);
			memcpy(received, in, len);
			received[len] = '\0';
			
			printf("收到指令: %s\n", received);
			if (command_parse(received, response, sizeof(response)) == 0) {
				// 发送响应内容，需要预留 LWS_PRE 空间
				unsigned char response_buf[LWS_PRE + MAX_PAYLOAD_SIZE];
				size_t resp_len = strlen(response);
				memcpy(&response_buf[LWS_PRE], response, resp_len);
				lws_write(wsi, &response_buf[LWS_PRE], resp_len, LWS_WRITE_TEXT);
			}
			free(received);
		}
		break;

	case LWS_CALLBACK_SERVER_WRITEABLE:
		// 从全局缓冲区拷贝最新数据到此会话
		pthread_mutex_lock(&global_msg_mutex);
		if (global_broadcast_len > 0) {
			memcpy(pss->msg, global_broadcast_msg, global_broadcast_len);
			pss->msg_len = global_broadcast_len;
		}
		pthread_mutex_unlock(&global_msg_mutex);

		// 发送数据
		if (pss->msg_len > 0) {
			unsigned char write_buf[LWS_PRE + MAX_PAYLOAD_SIZE];
			memcpy(&write_buf[LWS_PRE], pss->msg, pss->msg_len);
			lws_write(wsi, &write_buf[LWS_PRE], pss->msg_len, LWS_WRITE_TEXT);
			pss->msg_len = 0; // 发送完毕
		}
		break;

	case LWS_CALLBACK_CLOSED:
		printf("WebSocket 连接断开: %p\n", wsi);
		break;

	default:
		break;
	}

	return 0;
}

