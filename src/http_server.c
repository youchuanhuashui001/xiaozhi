#include "http_server.h"
#include <string.h>

static const struct lws_http_mount mount = {
	/* .mount_next */		NULL,		/* linked-list "next" */
	/* .mountpoint */		"/",		/* mountpoint URL */
	/* .origin */			"./web",	/* 静态文件根目录 */
	/* .def */			"index.html",	/* 默认文件 */
	/* .protocol */			NULL,
	/* .cgis */			NULL,
	/* .extra_mimetypes */		NULL,
	/* .interpret */		NULL,
	/* .cgi_timeout */		0,
	/* .cache_max_age */		0,
	/* .auth_mask */		0,
	/* .cache_revalid_check */	0,
	/* .cache_revalidate */		0,
	/* .cache_intermediaries */	0,
	/* .origin_protocol */		LWSMPRO_FILE,	/* files in a dir */
	/* .mountpoint_len */		1,		/* char count */
	/* .basic_auth_login_file */	NULL,
};

int callback_http(struct lws *wsi, enum lws_callback_reasons reason,
		  void *user, void *in, size_t len)
{
	(void)user;
	(void)in;
	(void)len;

	switch (reason) {
	case LWS_CALLBACK_HTTP:
		// 如果需要实现更复杂的动态内容，可以在这里处理
		// 否则，lws 会根据 mount 自动处理静态文件
		return 0;
	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}

struct lws_http_mount* http_server_get_mount(void)
{
	return (struct lws_http_mount*)&mount;
}
