#ifndef AUDIO_PLAYBACK_H
#define AUDIO_PLAYBACK_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

#include "audio_buffer.h"
#include "event_queue.h"

typedef struct {
	char device[64];
	int channels;
	int default_sample_rate;
	size_t queue_capacity;
	size_t max_frame_bytes;
} audio_playback_config_t;

typedef struct {
	audio_playback_config_t config;
	audio_buffer_t queue;
	event_queue_t *events;
	pthread_t thread;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int running;
	int stop_requested;
} audio_playback_t;

int audio_playback_init(audio_playback_t *pb,
			const audio_playback_config_t *cfg,
			event_queue_t *events);
int audio_playback_enqueue(audio_playback_t *pb, const int16_t *pcm,
			   size_t frames, int sample_rate);
void audio_playback_request_stop(audio_playback_t *pb);
void audio_playback_destroy(audio_playback_t *pb);

#endif /* AUDIO_PLAYBACK_H */
