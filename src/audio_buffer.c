#include "audio_buffer.h"

#include <stdlib.h>
#include <string.h>

int audio_buffer_init(audio_buffer_t *buffer, size_t capacity, size_t block_capacity)
{
	size_t i;

	if (!buffer || capacity == 0 || block_capacity == 0)
		return -1;

	memset(buffer, 0, sizeof(*buffer));
	buffer->blocks = calloc(capacity, sizeof(*buffer->blocks));
	buffer->storage = calloc(capacity, block_capacity);
	if (!buffer->blocks || !buffer->storage) {
		audio_buffer_destroy(buffer);
		return -1;
	}

	buffer->capacity = capacity;
	buffer->block_capacity = block_capacity;
	for (i = 0; i < capacity; i++)
		buffer->blocks[i].data = buffer->storage + i * block_capacity;

	pthread_mutex_init(&buffer->mutex, NULL);
	return 0;
}

int audio_buffer_push(audio_buffer_t *buffer, const uint8_t *data, size_t len, int sample_rate)
{
	audio_block_t *slot;

	if (!buffer || !data || len > buffer->block_capacity)
		return -1;

	pthread_mutex_lock(&buffer->mutex);
	if (buffer->count == buffer->capacity) {
		pthread_mutex_unlock(&buffer->mutex);
		return -1;
	}

	slot = &buffer->blocks[buffer->tail];
	memcpy(slot->data, data, len);
	slot->len = len;
	slot->sample_rate = sample_rate;
	buffer->tail = (buffer->tail + 1) % buffer->capacity;
	buffer->count++;
	pthread_mutex_unlock(&buffer->mutex);

	return 0;
}

int audio_buffer_pop(audio_buffer_t *buffer, audio_block_t *block)
{
	if (!buffer || !block)
		return -1;

	pthread_mutex_lock(&buffer->mutex);
	if (buffer->count == 0) {
		pthread_mutex_unlock(&buffer->mutex);
		return 0;
	}

	*block = buffer->blocks[buffer->head];
	buffer->head = (buffer->head + 1) % buffer->capacity;
	buffer->count--;
	pthread_mutex_unlock(&buffer->mutex);

	return 1;
}

size_t audio_buffer_size(audio_buffer_t *buffer)
{
	size_t count;

	if (!buffer)
		return 0;

	pthread_mutex_lock(&buffer->mutex);
	count = buffer->count;
	pthread_mutex_unlock(&buffer->mutex);

	return count;
}

void audio_buffer_destroy(audio_buffer_t *buffer)
{
	if (!buffer)
		return;

	free(buffer->blocks);
	free(buffer->storage);
	buffer->blocks = NULL;
	buffer->storage = NULL;
	buffer->capacity = 0;
	buffer->block_capacity = 0;
	buffer->head = 0;
	buffer->tail = 0;
	buffer->count = 0;
	pthread_mutex_destroy(&buffer->mutex);
}
