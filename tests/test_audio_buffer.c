#include <assert.h>
#include <stdint.h>

#include "audio_buffer.h"

int main(void)
{
	audio_buffer_t q;
	uint8_t frame[32] = { 1, 2, 3 };

	assert(audio_buffer_init(&q, 2, sizeof(frame)) == 0);
	assert(audio_buffer_push(&q, frame, sizeof(frame), 16000) == 0);
	assert(audio_buffer_size(&q) == 1);

	audio_buffer_destroy(&q);
	return 0;
}
