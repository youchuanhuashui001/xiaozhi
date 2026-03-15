#include <opus/opus.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void write_wav_header(FILE *out, uint32_t data_size, uint32_t sample_rate,
			     uint16_t channels, uint16_t bits_per_sample)
{
	uint32_t byte_rate = sample_rate * channels * bits_per_sample / 8;
	uint16_t block_align = channels * bits_per_sample / 8;
	uint32_t riff_size = 36 + data_size;
	uint8_t header[44];

	memset(header, 0, sizeof(header));
	memcpy(header + 0, "RIFF", 4);
	header[4] = (uint8_t)(riff_size & 0xFF);
	header[5] = (uint8_t)((riff_size >> 8) & 0xFF);
	header[6] = (uint8_t)((riff_size >> 16) & 0xFF);
	header[7] = (uint8_t)((riff_size >> 24) & 0xFF);
	memcpy(header + 8, "WAVE", 4);
	memcpy(header + 12, "fmt ", 4);
	header[16] = 16; /* PCM fmt chunk size */
	header[20] = 1;  /* PCM format */
	header[22] = (uint8_t)(channels & 0xFF);
	header[23] = (uint8_t)((channels >> 8) & 0xFF);
	header[24] = (uint8_t)(sample_rate & 0xFF);
	header[25] = (uint8_t)((sample_rate >> 8) & 0xFF);
	header[26] = (uint8_t)((sample_rate >> 16) & 0xFF);
	header[27] = (uint8_t)((sample_rate >> 24) & 0xFF);
	header[28] = (uint8_t)(byte_rate & 0xFF);
	header[29] = (uint8_t)((byte_rate >> 8) & 0xFF);
	header[30] = (uint8_t)((byte_rate >> 16) & 0xFF);
	header[31] = (uint8_t)((byte_rate >> 24) & 0xFF);
	header[32] = (uint8_t)(block_align & 0xFF);
	header[33] = (uint8_t)((block_align >> 8) & 0xFF);
	header[34] = (uint8_t)(bits_per_sample & 0xFF);
	header[35] = (uint8_t)((bits_per_sample >> 8) & 0xFF);
	memcpy(header + 36, "data", 4);
	header[40] = (uint8_t)(data_size & 0xFF);
	header[41] = (uint8_t)((data_size >> 8) & 0xFF);
	header[42] = (uint8_t)((data_size >> 16) & 0xFF);
	header[43] = (uint8_t)((data_size >> 24) & 0xFF);

	fseek(out, 0, SEEK_SET);
	fwrite(header, 1, sizeof(header), out);
}

int main(int argc, char **argv)
{
	const int sample_rate = 16000;
	const int channels = 1;
	const int16_t bits_per_sample = 16;
	const int max_frame_size = 5760;
	FILE *in = NULL;
	FILE *out = NULL;
	OpusDecoder *decoder = NULL;
	int opus_err = OPUS_OK;
	uint32_t data_size = 0;
	uint8_t len_buf[2];
	uint8_t *packet = NULL;
	size_t packet_capacity = 0;
	int16_t pcm[5760];

	if (argc != 3) {
		fprintf(stderr, "usage: %s <input.opusbin> <output.wav>\n", argv[0]);
		return 1;
	}

	in = fopen(argv[1], "rb");
	if (!in) {
		perror("open input");
		return 1;
	}

	out = fopen(argv[2], "wb+");
	if (!out) {
		perror("open output");
		fclose(in);
		return 1;
	}

	/* Placeholder header, updated at the end once data_size is known. */
	write_wav_header(out, 0, (uint32_t)sample_rate, (uint16_t)channels, bits_per_sample);

	decoder = opus_decoder_create(sample_rate, channels, &opus_err);
	if (!decoder || opus_err != OPUS_OK) {
		fprintf(stderr, "opus_decoder_create failed: %d\n", opus_err);
		fclose(in);
		fclose(out);
		return 1;
	}

	while (fread(len_buf, 1, sizeof(len_buf), in) == sizeof(len_buf)) {
		uint16_t packet_len = (uint16_t)(len_buf[0] | ((uint16_t)len_buf[1] << 8));
		int decoded_samples;
		size_t pcm_bytes;

		if (packet_len == 0)
			continue;

		if (packet_len > packet_capacity) {
			uint8_t *new_buf = realloc(packet, packet_len);
			if (!new_buf) {
				fprintf(stderr, "out of memory while growing packet buffer\n");
				free(packet);
				opus_decoder_destroy(decoder);
				fclose(in);
				fclose(out);
				return 1;
			}
			packet = new_buf;
			packet_capacity = packet_len;
		}

		if (fread(packet, 1, packet_len, in) != packet_len) {
			fprintf(stderr, "truncated packet payload\n");
			break;
		}

		decoded_samples = opus_decode(decoder, packet, (opus_int32)packet_len,
					      pcm, max_frame_size, 0);
		if (decoded_samples < 0) {
			fprintf(stderr, "opus_decode failed: %s\n", opus_strerror(decoded_samples));
			continue;
		}

		pcm_bytes = (size_t)decoded_samples * (size_t)channels * sizeof(int16_t);
		if (fwrite(pcm, 1, pcm_bytes, out) != pcm_bytes) {
			fprintf(stderr, "failed to write decoded pcm\n");
			break;
		}
		data_size += (uint32_t)pcm_bytes;
	}

	write_wav_header(out, data_size, (uint32_t)sample_rate, (uint16_t)channels, bits_per_sample);

	free(packet);
	opus_decoder_destroy(decoder);
	fclose(in);
	fclose(out);

	printf("decoded %u bytes pcm to %s\n", data_size, argv[2]);
	return 0;
}
