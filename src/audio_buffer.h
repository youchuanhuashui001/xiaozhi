#ifndef AUDIO_BUFFER_H
#define AUDIO_BUFFER_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
	uint8_t *data;
	size_t len;
	int sample_rate;
} audio_block_t;

typedef struct {
	audio_block_t *blocks;
	uint8_t *storage;
	size_t capacity;
	size_t block_capacity;
	size_t head;
	size_t tail;
	size_t count;
	pthread_mutex_t mutex;
} audio_buffer_t;

int audio_buffer_init(audio_buffer_t *buffer, size_t capacity, size_t block_capacity);
int audio_buffer_push(audio_buffer_t *buffer, const uint8_t *data, size_t len, int sample_rate);
int audio_buffer_pop(audio_buffer_t *buffer, audio_block_t *block);
size_t audio_buffer_size(audio_buffer_t *buffer);
void audio_buffer_destroy(audio_buffer_t *buffer);

#endif /* AUDIO_BUFFER_H */
