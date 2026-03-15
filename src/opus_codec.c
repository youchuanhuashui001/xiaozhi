#include "opus_codec.h"

#include <string.h>

int opus_encoder_wrapper_init(opus_encoder_wrapper_t *enc, int sample_rate,
			      int channels, int frame_ms)
{
	int err;

	if (!enc || sample_rate <= 0 || channels <= 0 || frame_ms <= 0)
		return -1;

	memset(enc, 0, sizeof(*enc));
	enc->encoder = opus_encoder_create(sample_rate, channels,
					   OPUS_APPLICATION_VOIP, &err);
	if (!enc->encoder || err != OPUS_OK)
		return -1;

	enc->sample_rate = sample_rate;
	enc->channels = channels;
	enc->frame_ms = frame_ms;
	enc->samples_per_channel = (sample_rate * frame_ms) / 1000;
	return 0;
}

int opus_encode_frame(opus_encoder_wrapper_t *enc, const int16_t *pcm,
		      int samples_per_channel, uint8_t *out, size_t out_size)
{
	if (!enc || !enc->encoder || !pcm || !out || out_size == 0)
		return -1;

	if (samples_per_channel != enc->samples_per_channel)
		return -1;

	return opus_encode(enc->encoder, pcm, samples_per_channel, out,
			   (opus_int32)out_size);
}

void opus_encoder_wrapper_destroy(opus_encoder_wrapper_t *enc)
{
	if (!enc)
		return;

	if (enc->encoder)
		opus_encoder_destroy(enc->encoder);
	memset(enc, 0, sizeof(*enc));
}

int opus_decoder_wrapper_init(opus_decoder_wrapper_t *dec, int sample_rate,
			      int channels)
{
	int err;

	if (!dec || sample_rate <= 0 || channels <= 0)
		return -1;

	memset(dec, 0, sizeof(*dec));
	dec->decoder = opus_decoder_create(sample_rate, channels, &err);
	if (!dec->decoder || err != OPUS_OK)
		return -1;

	dec->sample_rate = sample_rate;
	dec->channels = channels;
	return 0;
}

int opus_decode_frame(opus_decoder_wrapper_t *dec, const uint8_t *packet,
		      size_t packet_len, int16_t *pcm_out, int pcm_capacity)
{
	int frame_capacity;

	if (!dec || !dec->decoder || !packet || !pcm_out || packet_len == 0)
		return -1;

	if (dec->channels <= 0)
		return -1;

	frame_capacity = pcm_capacity / dec->channels;
	if (frame_capacity <= 0)
		return -1;

	return opus_decode(dec->decoder, packet, (opus_int32)packet_len, pcm_out,
			   frame_capacity, 0);
}

void opus_decoder_wrapper_destroy(opus_decoder_wrapper_t *dec)
{
	if (!dec)
		return;

	if (dec->decoder)
		opus_decoder_destroy(dec->decoder);
	memset(dec, 0, sizeof(*dec));
}
