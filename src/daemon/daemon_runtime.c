#include "daemon/daemon_runtime.h"

#include <stdio.h>
#include <string.h>

#include "control_plane/control_events.h"

static int daemon_runtime_copy_string(char *dst, size_t dst_size,
					 const char *src)
{
	if (!dst || dst_size == 0)
		return -1;

	dst[0] = '\0';
	if (!src)
		return 0;

	if (snprintf(dst, dst_size, "%s", src) >= (int)dst_size)
		return -1;

	return 0;
}

static daemon_runtime_command_t daemon_runtime_command_from_name(const char *name)
{
	if (!name || name[0] == '\0')
		return DAEMON_RUNTIME_COMMAND_UNKNOWN;
	if (strcmp(name, "connect_server") == 0)
		return DAEMON_RUNTIME_COMMAND_CONNECT_SERVER;
	if (strcmp(name, "disconnect_server") == 0)
		return DAEMON_RUNTIME_COMMAND_DISCONNECT_SERVER;
	if (strcmp(name, "set_server_config") == 0)
		return DAEMON_RUNTIME_COMMAND_SET_SERVER_CONFIG;
	if (strcmp(name, "set_audio_config") == 0)
		return DAEMON_RUNTIME_COMMAND_SET_AUDIO_CONFIG;
	if (strcmp(name, "test_connection") == 0)
		return DAEMON_RUNTIME_COMMAND_TEST_CONNECTION;
	if (strcmp(name, "shutdown_client") == 0)
		return DAEMON_RUNTIME_COMMAND_SHUTDOWN_CLIENT;
	return DAEMON_RUNTIME_COMMAND_UNKNOWN;
}

static int daemon_runtime_capture_state_event(daemon_runtime_t *rt,
					      session_state_t state)
{
	const char *state_name;

	if (!rt)
		return -1;

	memset(&rt->last_control_event, 0, sizeof(rt->last_control_event));
	if (daemon_runtime_copy_string(rt->last_control_event.type,
				       sizeof(rt->last_control_event.type),
				       "event") != 0 ||
	    daemon_runtime_copy_string(rt->last_control_event.name,
				       sizeof(rt->last_control_event.name),
				       "state_changed") != 0)
		return -1;

	state_name = control_events_state_name(state);
	rt->last_control_event.ok = 1;
	if (snprintf(rt->last_control_event.payload,
		     sizeof(rt->last_control_event.payload),
		     "{\"state\":\"%s\"}", state_name) >=
	    (int)sizeof(rt->last_control_event.payload))
		return -1;

	return 0;
}

static int daemon_runtime_capture_error_event(daemon_runtime_t *rt,
					      const app_observer_event_t *event)
{
	if (!rt || !event)
		return -1;

	memset(&rt->last_control_event, 0, sizeof(rt->last_control_event));
	if (daemon_runtime_copy_string(rt->last_control_event.type,
				       sizeof(rt->last_control_event.type),
				       "event") != 0 ||
	    daemon_runtime_copy_string(rt->last_control_event.name,
				       sizeof(rt->last_control_event.name),
				       "runtime_error") != 0 ||
	    daemon_runtime_copy_string(rt->last_control_event.message,
				       sizeof(rt->last_control_event.message),
				       event->text) != 0)
		return -1;

	rt->last_control_event.ok = 0;
	return 0;
}

static void daemon_runtime_on_app_event(const app_observer_event_t *event, void *ctx)
{
	daemon_runtime_t *rt = ctx;
	control_event_t mapped;

	if (!rt || !event)
		return;

	pthread_mutex_lock(&rt->lock);
	rt->observed_event_count++;

	switch (event->kind) {
	case APP_OBSERVER_EVENT_STATE_CHANGED:
		(void)daemon_runtime_capture_state_event(rt, event->state);
		break;
	case APP_OBSERVER_EVENT_PROTOCOL:
		memset(&mapped, 0, sizeof(mapped));
		if (control_events_from_protocol(&event->protocol, &mapped) == 0)
			rt->last_control_event = mapped;
		break;
	case APP_OBSERVER_EVENT_ERROR:
		(void)daemon_runtime_capture_error_event(rt, event);
		break;
	case APP_OBSERVER_EVENT_NONE:
	default:
		break;
	}
	pthread_mutex_unlock(&rt->lock);
}

int daemon_runtime_init(daemon_runtime_t *rt, app_runtime_t *app)
{
	int rc;

	if (!rt)
		return -1;

	memset(rt, 0, sizeof(*rt));
	rt->app = app;
	if (pthread_mutex_init(&rt->lock, NULL) != 0)
		return -1;

	if (!app)
		return 0;

	rc = app_set_observer(app, daemon_runtime_on_app_event, rt);
	if (rc != 0)
		pthread_mutex_destroy(&rt->lock);
	return rc;
}

int daemon_runtime_submit_command(daemon_runtime_t *rt,
				     const control_command_t *cmd)
{
	daemon_runtime_command_t mapped;
	int rc = 0;
	int app_ready = 0;

	if (!rt || !cmd)
		return -1;

	mapped = daemon_runtime_command_from_name(cmd->name);
	if (mapped == DAEMON_RUNTIME_COMMAND_UNKNOWN)
		return -1;

	pthread_mutex_lock(&rt->lock);
	rt->last_command = mapped;
	rt->last_control_command = *cmd;
	pthread_mutex_unlock(&rt->lock);

	app_ready = rt->app && rt->app->initialized && !rt->app->check_only &&
		    !rt->app->skip_runtime_init;
	if (!app_ready)
		return 0;

	switch (mapped) {
	case DAEMON_RUNTIME_COMMAND_CONNECT_SERVER:
		rc = app_control_connect(rt->app);
		break;
	case DAEMON_RUNTIME_COMMAND_DISCONNECT_SERVER:
		rc = app_control_disconnect(rt->app);
		break;
	case DAEMON_RUNTIME_COMMAND_SHUTDOWN_CLIENT:
		rc = app_control_shutdown(rt->app);
		break;
	case DAEMON_RUNTIME_COMMAND_SET_SERVER_CONFIG:
	case DAEMON_RUNTIME_COMMAND_SET_AUDIO_CONFIG:
	case DAEMON_RUNTIME_COMMAND_TEST_CONNECTION:
	case DAEMON_RUNTIME_COMMAND_NONE:
	default:
		rc = 0;
		break;
	}

	return rc == 0 ? 0 : -1;
}

int daemon_runtime_snapshot(daemon_runtime_t *rt, control_event_t *event,
			    unsigned long *observed_event_count)
{
	if (!rt)
		return -1;

	pthread_mutex_lock(&rt->lock);
	if (event)
		*event = rt->last_control_event;
	if (observed_event_count)
		*observed_event_count = rt->observed_event_count;
	pthread_mutex_unlock(&rt->lock);
	return 0;
}

void daemon_runtime_destroy(daemon_runtime_t *rt)
{
	if (!rt)
		return;

	if (rt->app && rt->app->observer_ctx == rt)
		(void)app_set_observer(rt->app, NULL, NULL);

	pthread_mutex_destroy(&rt->lock);
	memset(rt, 0, sizeof(*rt));
}
