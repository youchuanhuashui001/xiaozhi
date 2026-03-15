#ifndef OPUS_CODEC_H
#define OPUS_CODEC_H

#include <stddef.h>
#include <stdint.h>

#include <opus/opus.h>

typedef struct {
	OpusEncoder *encoder;
	int sample_rate;
	int channels;
	int frame_ms;
	int samples_per_channel;
} opus_encoder_wrapper_t;

typedef struct {
	OpusDecoder *decoder;
	int sample_rate;
	int channels;
} opus_decoder_wrapper_t;

int opus_encoder_wrapper_init(opus_encoder_wrapper_t *enc, int sample_rate,
			      int channels, int frame_ms);
int opus_encode_frame(opus_encoder_wrapper_t *enc, const int16_t *pcm,
		      int samples_per_channel, uint8_t *out, size_t out_size);
void opus_encoder_wrapper_destroy(opus_encoder_wrapper_t *enc);

int opus_decoder_wrapper_init(opus_decoder_wrapper_t *dec, int sample_rate,
			      int channels);
int opus_decode_frame(opus_decoder_wrapper_t *dec, const uint8_t *packet,
		      size_t packet_len, int16_t *pcm_out, int pcm_capacity);
void opus_decoder_wrapper_destroy(opus_decoder_wrapper_t *dec);

#endif /* OPUS_CODEC_H */
