#include <assert.h>
#include <stdint.h>

#include "opus_codec.h"

int main(void)
{
	opus_encoder_wrapper_t enc;
	opus_decoder_wrapper_t dec;
	int16_t pcm[960] = {0};
	uint8_t packet[256];
	int16_t decoded[1920];
	int packet_len;

	assert(opus_encoder_wrapper_init(&enc, 16000, 1, 60) == 0);
	assert(opus_decoder_wrapper_init(&dec, 24000, 1) == 0);

	packet_len = opus_encode_frame(&enc, pcm, 960, packet, sizeof(packet));
	assert(packet_len > 0);
	assert(opus_decode_frame(&dec, packet, (size_t)packet_len, decoded, 1920) >= 0);

	opus_encoder_wrapper_destroy(&enc);
	opus_decoder_wrapper_destroy(&dec);
	return 0;
}
