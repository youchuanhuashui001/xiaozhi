#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static log_level_t current_level = LOG_LEVEL_INFO;

static const char *log_level_name(log_level_t level)
{
	switch (level) {
	case LOG_LEVEL_ERROR:
		return "ERROR";
	case LOG_LEVEL_WARN:
		return "WARN";
	case LOG_LEVEL_INFO:
		return "INFO";
	case LOG_LEVEL_DEBUG:
		return "DEBUG";
	default:
		return "UNKNOWN";
	}
}

static void log_write(log_level_t level, const char *fmt, va_list ap)
{
	time_t now;
	struct tm tm_now;
	char ts[20];

	if (level > current_level)
		return;

	now = time(NULL);
	localtime_r(&now, &tm_now);
	strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_now);

	fprintf(stderr, "[%s] %s ", ts, log_level_name(level));
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
}

void log_set_level(log_level_t level)
{
	current_level = level;
}

void log_error(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	log_write(LOG_LEVEL_ERROR, fmt, ap);
	va_end(ap);
}

void log_warn(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	log_write(LOG_LEVEL_WARN, fmt, ap);
	va_end(ap);
}

void log_info(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	log_write(LOG_LEVEL_INFO, fmt, ap);
	va_end(ap);
}

void log_debug(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	log_write(LOG_LEVEL_DEBUG, fmt, ap);
	va_end(ap);
}
