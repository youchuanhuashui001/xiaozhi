#include "audio_capture.h"

#include <alsa/asoundlib.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static int16_t max_abs_sample(const int16_t *pcm, size_t frames, int channels)
{
	size_t i;
	int16_t peak = 0;

	for (i = 0; i < frames * (size_t)channels; i++) {
		int sample = pcm[i];
		int16_t abs_sample = (int16_t)(sample < 0 ? -sample : sample);

		if (abs_sample > peak)
			peak = abs_sample;
	}

	return peak;
}

static void push_silence_timeout(audio_capture_t *cap)
{
	app_event_t event;

	if (!cap->events)
		return;

	memset(&event, 0, sizeof(event));
	event.type = APP_EVENT_CAPTURE_SILENCE_TIMEOUT;
	(void)event_queue_push(cap->events, &event);
}

static int audio_capture_open_device(audio_capture_t *cap, snd_pcm_t **handle)
{
	snd_pcm_hw_params_t *params = NULL;
	snd_pcm_uframes_t period_frames;
	int rc;

	rc = snd_pcm_open(handle, cap->config.device[0] ? cap->config.device : "default",
			  SND_PCM_STREAM_CAPTURE, 0);
	if (rc < 0)
		return rc;

	if (snd_pcm_hw_params_malloc(&params) < 0)
		goto fail;
	if (snd_pcm_hw_params_any(*handle, params) < 0)
		goto fail;
	if (snd_pcm_hw_params_set_access(*handle, params,
					 SND_PCM_ACCESS_RW_INTERLEAVED) < 0)
		goto fail;
	if (snd_pcm_hw_params_set_format(*handle, params, SND_PCM_FORMAT_S16_LE) < 0)
		goto fail;
	if (snd_pcm_hw_params_set_channels(*handle, params,
					   (unsigned int)cap->config.channels) < 0)
		goto fail;
	if (snd_pcm_hw_params_set_rate(*handle, params,
				       (unsigned int)cap->config.sample_rate, 0) < 0)
		goto fail;
	period_frames = (snd_pcm_uframes_t)cap->config.period_frames;
	if (snd_pcm_hw_params_set_period_size_near(*handle, params,
						   &period_frames, NULL) < 0)
		goto fail;
	rc = snd_pcm_hw_params(*handle, params);
	snd_pcm_hw_params_free(params);
	if (rc < 0) {
		snd_pcm_close(*handle);
		*handle = NULL;
		return rc;
	}
	cap->config.period_frames = (size_t)period_frames;

	return 0;

fail:
	if (params)
		snd_pcm_hw_params_free(params);
	snd_pcm_close(*handle);
	*handle = NULL;
	return -1;
}

static void *audio_capture_thread(void *arg)
{
	audio_capture_t *cap = arg;
	snd_pcm_t *pcm = NULL;
	int16_t *buffer = NULL;
	size_t sample_count;

	if (audio_capture_open_device(cap, &pcm) != 0)
		return NULL;

	sample_count = cap->config.period_frames * (size_t)cap->config.channels;
	buffer = calloc(sample_count, sizeof(*buffer));
	if (!buffer) {
		snd_pcm_close(pcm);
		return NULL;
	}

	while (1) {
		int uploading;
		snd_pcm_sframes_t frames_read;

		pthread_mutex_lock(&cap->mutex);
		uploading = cap->uploading;
		if (!cap->running) {
			pthread_mutex_unlock(&cap->mutex);
			break;
		}
		pthread_mutex_unlock(&cap->mutex);

		frames_read = snd_pcm_readi(pcm, buffer, cap->config.period_frames);
		if (frames_read < 0) {
			snd_pcm_prepare(pcm);
			continue;
		}

		if (cap->pcm_cb)
			cap->pcm_cb(cap->pcm_cb_ctx, buffer, (size_t)frames_read);

		if (uploading && cap->config.silence_timeout_ms > 0) {
			int chunk_ms = (int)(((long long)frames_read * 1000) /
				cap->config.sample_rate);

			if (max_abs_sample(buffer, (size_t)frames_read, cap->config.channels) <=
			    cap->config.silence_threshold) {
				cap->silence_accumulator_ms += chunk_ms;
				if (cap->silence_accumulator_ms >= cap->config.silence_timeout_ms) {
					push_silence_timeout(cap);
					cap->silence_accumulator_ms = 0;
				}
			} else {
				cap->silence_accumulator_ms = 0;
			}
		} else {
			cap->silence_accumulator_ms = 0;
		}
	}

	free(buffer);
	snd_pcm_drop(pcm);
	snd_pcm_close(pcm);
	return NULL;
}

int audio_capture_start(audio_capture_t *cap, const audio_capture_config_t *cfg,
			audio_capture_pcm_cb cb, void *cb_ctx,
			event_queue_t *events)
{
	audio_capture_config_t defaults;

	if (!cap)
		return -1;

	memset(cap, 0, sizeof(*cap));
	memset(&defaults, 0, sizeof(defaults));
	snprintf(defaults.device, sizeof(defaults.device), "%s", "default");
	defaults.sample_rate = 16000;
	defaults.channels = 1;
	defaults.period_frames = 960;
	defaults.silence_timeout_ms = 1200;
	defaults.silence_threshold = 500;
	cap->config = defaults;
	if (cfg)
		cap->config = *cfg;

	if (cap->config.sample_rate <= 0)
		cap->config.sample_rate = 16000;
	if (cap->config.channels <= 0)
		cap->config.channels = 1;
	if (cap->config.period_frames == 0)
		cap->config.period_frames = 960;
	if (cap->config.silence_timeout_ms < 0)
		cap->config.silence_timeout_ms = 1200;
	if (cap->config.silence_threshold < 0)
		cap->config.silence_threshold = 500;
	if (cap->config.device[0] == '\0')
		snprintf(cap->config.device, sizeof(cap->config.device), "%s", "default");

	cap->pcm_cb = cb;
	cap->pcm_cb_ctx = cb_ctx;
	cap->events = events;
	cap->running = 1;
	pthread_mutex_init(&cap->mutex, NULL);

	if (pthread_create(&cap->thread, NULL, audio_capture_thread, cap) != 0) {
		pthread_mutex_destroy(&cap->mutex);
		memset(cap, 0, sizeof(*cap));
		return -1;
	}

	return 0;
}

void audio_capture_set_uploading(audio_capture_t *cap, int enabled)
{
	if (!cap)
		return;

	pthread_mutex_lock(&cap->mutex);
	cap->uploading = enabled ? 1 : 0;
	if (!cap->uploading)
		cap->silence_accumulator_ms = 0;
	pthread_mutex_unlock(&cap->mutex);
}

void audio_capture_stop(audio_capture_t *cap)
{
	if (!cap)
		return;

	pthread_mutex_lock(&cap->mutex);
	cap->running = 0;
	pthread_mutex_unlock(&cap->mutex);

	if (cap->thread)
		pthread_join(cap->thread, NULL);
	pthread_mutex_destroy(&cap->mutex);
	memset(cap, 0, sizeof(*cap));
}
