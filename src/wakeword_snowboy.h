#ifndef WAKEWORD_SNOWBOY_H
#define WAKEWORD_SNOWBOY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wakeword_snowboy {
	void *impl;
	int sample_rate;
	int channels;
	int bits_per_sample;
} wakeword_snowboy_t;

int wakeword_snowboy_init(wakeword_snowboy_t *detector, const char *resource_path,
			  const char *model_path, float sensitivity,
			  float audio_gain);
int wakeword_snowboy_feed(wakeword_snowboy_t *detector, const int16_t *pcm,
			  size_t samples);
void wakeword_snowboy_destroy(wakeword_snowboy_t *detector);

#ifdef __cplusplus
}
#endif

#endif /* WAKEWORD_SNOWBOY_H */
