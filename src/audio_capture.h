#ifndef AUDIO_CAPTURE_H
#define AUDIO_CAPTURE_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

#include "event_queue.h"

typedef void (*audio_capture_pcm_cb)(void *ctx, const int16_t *pcm, size_t frames);

typedef struct {
	char device[64];
	int sample_rate;
	int channels;
	size_t period_frames;
	int silence_timeout_ms;
	int silence_threshold;
} audio_capture_config_t;

typedef struct {
	audio_capture_config_t config;
	audio_capture_pcm_cb pcm_cb;
	void *pcm_cb_ctx;
	event_queue_t *events;
	pthread_t thread;
	pthread_mutex_t mutex;
	int running;
	int uploading;
	int silence_accumulator_ms;
} audio_capture_t;

int audio_capture_start(audio_capture_t *cap, const audio_capture_config_t *cfg,
			audio_capture_pcm_cb cb, void *cb_ctx,
			event_queue_t *events);
void audio_capture_set_uploading(audio_capture_t *cap, int enabled);
void audio_capture_update_silence(audio_capture_t *cap, int timeout_ms, int threshold);
void audio_capture_stop(audio_capture_t *cap);

#endif /* AUDIO_CAPTURE_H */
