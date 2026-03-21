#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "audio_playback_internal.h"

typedef struct {
	long returns[8];
	size_t return_count;
	size_t return_index;
	size_t total_frames_requested;
	int recover_calls;
} playback_stub_t;

static long stub_write_frames(void *ctx, const int16_t *pcm, size_t frames)
{
	playback_stub_t *stub = ctx;

	(void)pcm;
	stub->total_frames_requested += frames;
	if (stub->return_index >= stub->return_count)
		return -1;
	return stub->returns[stub->return_index++];
}

static int stub_recover(void *ctx, int err)
{
	playback_stub_t *stub = ctx;

	(void)err;
	stub->recover_calls++;
	return 0;
}

static void test_write_all_frames_handles_partial_writes(void)
{
	playback_stub_t stub = {
		.returns = { 2, 3 },
		.return_count = 2
	};
	int16_t pcm[5] = {1, 2, 3, 4, 5};

	assert(audio_playback_write_all_frames(pcm, 5, 1,
					       stub_write_frames, stub_recover,
					       &stub) == 0);
	assert(stub.return_index == 2);
}

static void test_write_all_frames_recovers_then_continues(void)
{
	playback_stub_t stub = {
		.returns = { -32, 5 },
		.return_count = 2
	};
	int16_t pcm[5] = {1, 2, 3, 4, 5};

	assert(audio_playback_write_all_frames(pcm, 5, 1,
					       stub_write_frames, stub_recover,
					       &stub) == 0);
	assert(stub.recover_calls == 1);
	assert(stub.return_index == 2);
}

static void test_start_threshold_prefers_three_periods(void)
{
	assert(audio_playback_compute_start_threshold(8192, 1024, 3) == 3072);
}

static void test_start_threshold_is_clamped_to_buffer_size(void)
{
	assert(audio_playback_compute_start_threshold(2000, 900, 3) == 2000);
}

static void test_start_threshold_falls_back_to_full_buffer_when_period_unknown(void)
{
	assert(audio_playback_compute_start_threshold(4096, 0, 3) == 4096);
}

int main(void)
{
	test_write_all_frames_handles_partial_writes();
	test_write_all_frames_recovers_then_continues();
	test_start_threshold_prefers_three_periods();
	test_start_threshold_is_clamped_to_buffer_size();
	test_start_threshold_falls_back_to_full_buffer_when_period_unknown();
	return 0;
}
