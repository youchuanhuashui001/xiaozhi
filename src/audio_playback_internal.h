#ifndef AUDIO_PLAYBACK_INTERNAL_H
#define AUDIO_PLAYBACK_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

typedef long (*audio_playback_write_frames_fn)(void *ctx, const int16_t *pcm,
						size_t frames);
typedef int (*audio_playback_recover_fn)(void *ctx, int err);

int audio_playback_write_all_frames(const int16_t *pcm, size_t frames, int channels,
				    audio_playback_write_frames_fn write_frames,
				    audio_playback_recover_fn recover,
				    void *ctx);
size_t audio_playback_compute_start_threshold(size_t buffer_frames,
					      size_t period_frames,
					      size_t periods_to_buffer);

#endif /* AUDIO_PLAYBACK_INTERNAL_H */
