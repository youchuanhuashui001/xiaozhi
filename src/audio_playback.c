#include "audio_playback.h"

#include <alsa/asoundlib.h>
#include <string.h>
#include <unistd.h>

static void push_playback_finished(audio_playback_t *pb)
{
	app_event_t event;

	if (!pb->events)
		return;

	memset(&event, 0, sizeof(event));
	event.type = APP_EVENT_PLAYBACK_FINISHED;
	(void)event_queue_push(pb->events, &event);
}

static int audio_playback_open_device(audio_playback_t *pb, int sample_rate,
				      snd_pcm_t **handle)
{
	snd_pcm_hw_params_t *params = NULL;
	int rc;

	rc = snd_pcm_open(handle, pb->config.device[0] ? pb->config.device : "default",
			  SND_PCM_STREAM_PLAYBACK, 0);
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
					   (unsigned int)pb->config.channels) < 0)
		goto fail;
	if (snd_pcm_hw_params_set_rate(*handle, params,
				       (unsigned int)sample_rate, 0) < 0)
		goto fail;
	rc = snd_pcm_hw_params(*handle, params);
	snd_pcm_hw_params_free(params);
	if (rc < 0) {
		snd_pcm_close(*handle);
		*handle = NULL;
		return rc;
	}

	return 0;

fail:
	if (params)
		snd_pcm_hw_params_free(params);
	snd_pcm_close(*handle);
	*handle = NULL;
	return -1;
}

static void *audio_playback_thread(void *arg)
{
	audio_playback_t *pb = arg;
	snd_pcm_t *pcm = NULL;
	int current_rate = 0;

	while (1) {
		audio_block_t block;
		int should_stop = 0;

		pthread_mutex_lock(&pb->mutex);
		while (pb->running && audio_buffer_size(&pb->queue) == 0 && !pb->stop_requested)
			pthread_cond_wait(&pb->cond, &pb->mutex);
		if (!pb->running)
			should_stop = 1;
		pthread_mutex_unlock(&pb->mutex);

		if (should_stop)
			break;

		if (pb->stop_requested) {
			pb->stop_requested = 0;
			if (pcm) {
				snd_pcm_drop(pcm);
				snd_pcm_close(pcm);
				pcm = NULL;
				current_rate = 0;
			}
			push_playback_finished(pb);
			continue;
		}

		if (audio_buffer_pop(&pb->queue, &block) != 1) {
			usleep(1000);
			continue;
		}

		if (!pcm || current_rate != block.sample_rate) {
			if (pcm) {
				snd_pcm_drop(pcm);
				snd_pcm_close(pcm);
			}
			if (audio_playback_open_device(pb, block.sample_rate, &pcm) != 0) {
				pcm = NULL;
				current_rate = 0;
				continue;
			}
			current_rate = block.sample_rate;
		}

		if (pcm) {
			snd_pcm_sframes_t frames = (snd_pcm_sframes_t)(block.len /
				(sizeof(int16_t) * (size_t)pb->config.channels));
			snd_pcm_sframes_t rc = snd_pcm_writei(pcm, block.data, frames);

			if (rc < 0)
				snd_pcm_prepare(pcm);
			else if (audio_buffer_size(&pb->queue) == 0)
				push_playback_finished(pb);
		}
	}

	if (pcm) {
		snd_pcm_drop(pcm);
		snd_pcm_close(pcm);
	}

	return NULL;
}

int audio_playback_init(audio_playback_t *pb,
			const audio_playback_config_t *cfg,
			event_queue_t *events)
{
	audio_playback_config_t defaults;

	if (!pb)
		return -1;

	memset(pb, 0, sizeof(*pb));
	memset(&defaults, 0, sizeof(defaults));
	defaults.channels = 1;
	defaults.default_sample_rate = 24000;
	defaults.queue_capacity = 16;
	defaults.max_frame_bytes = 4096;
	snprintf(defaults.device, sizeof(defaults.device), "%s", "default");
	pb->config = defaults;
	if (cfg)
		pb->config = *cfg;

	if (pb->config.channels <= 0)
		pb->config.channels = 1;
	if (pb->config.queue_capacity == 0)
		pb->config.queue_capacity = 16;
	if (pb->config.max_frame_bytes == 0)
		pb->config.max_frame_bytes = 4096;
	if (pb->config.device[0] == '\0')
		snprintf(pb->config.device, sizeof(pb->config.device), "%s", "default");

	if (audio_buffer_init(&pb->queue, pb->config.queue_capacity,
			      pb->config.max_frame_bytes) != 0)
		return -1;

	pb->events = events;
	pthread_mutex_init(&pb->mutex, NULL);
	pthread_cond_init(&pb->cond, NULL);
	pb->running = 1;

	if (pthread_create(&pb->thread, NULL, audio_playback_thread, pb) != 0) {
		audio_buffer_destroy(&pb->queue);
		pthread_cond_destroy(&pb->cond);
		pthread_mutex_destroy(&pb->mutex);
		return -1;
	}

	return 0;
}

int audio_playback_enqueue(audio_playback_t *pb, const int16_t *pcm,
			   size_t frames, int sample_rate)
{
	size_t bytes;
	int rc;

	if (!pb || !pcm || frames == 0)
		return -1;

	bytes = frames * sizeof(int16_t) * (size_t)pb->config.channels;
	rc = audio_buffer_push(&pb->queue, (const uint8_t *)pcm, bytes, sample_rate);
	if (rc == 0) {
		pthread_mutex_lock(&pb->mutex);
		pthread_cond_signal(&pb->cond);
		pthread_mutex_unlock(&pb->mutex);
	}

	return rc;
}

void audio_playback_request_stop(audio_playback_t *pb)
{
	if (!pb)
		return;

	pthread_mutex_lock(&pb->mutex);
	pb->stop_requested = 1;
	pthread_cond_signal(&pb->cond);
	pthread_mutex_unlock(&pb->mutex);
}

void audio_playback_destroy(audio_playback_t *pb)
{
	if (!pb)
		return;

	pthread_mutex_lock(&pb->mutex);
	pb->running = 0;
	pthread_cond_signal(&pb->cond);
	pthread_mutex_unlock(&pb->mutex);

	if (pb->thread)
		pthread_join(pb->thread, NULL);

	audio_buffer_destroy(&pb->queue);
	pthread_cond_destroy(&pb->cond);
	pthread_mutex_destroy(&pb->mutex);
	memset(pb, 0, sizeof(*pb));
}
