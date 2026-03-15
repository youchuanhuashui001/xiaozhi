#include "wakeword_snowboy.h"

#include <exception>
#include <memory>
#include <new>
#include <string>

#include "snowboy-detect.h"

struct WakewordSnowboyImpl {
  std::unique_ptr<snowboy::SnowboyDetect> detector;
};

extern "C" {

int wakeword_snowboy_init(wakeword_snowboy_t *detector, const char *resource_path,
                          const char *model_path, float sensitivity,
                          float audio_gain)
{
  WakewordSnowboyImpl *impl;

  if (!detector || !resource_path || !model_path)
    return -1;

  detector->impl = nullptr;
  detector->sample_rate = 0;
  detector->channels = 0;
  detector->bits_per_sample = 0;

  try {
    impl = new WakewordSnowboyImpl();
    impl->detector.reset(new snowboy::SnowboyDetect(
        std::string(resource_path), std::string(model_path)));
    impl->detector->SetSensitivity(std::to_string(sensitivity));
    impl->detector->SetAudioGain(audio_gain);
    impl->detector->ApplyFrontend(false);
  } catch (const std::exception &) {
    return -1;
  } catch (...) {
    return -1;
  }

  detector->impl = impl;
  detector->sample_rate = impl->detector->SampleRate();
  detector->channels = impl->detector->NumChannels();
  detector->bits_per_sample = impl->detector->BitsPerSample();
  return 0;
}

int wakeword_snowboy_feed(wakeword_snowboy_t *detector, const int16_t *pcm,
                          size_t samples)
{
  WakewordSnowboyImpl *impl;
  int result;

  if (!detector || !detector->impl || !pcm || samples == 0)
    return -1;

  impl = static_cast<WakewordSnowboyImpl *>(detector->impl);
  result = impl->detector->RunDetection(pcm, static_cast<int>(samples), false);
  if (result < 0)
    return result;
  return result > 0 ? 1 : 0;
}

void wakeword_snowboy_destroy(wakeword_snowboy_t *detector)
{
  if (!detector)
    return;

  delete static_cast<WakewordSnowboyImpl *>(detector->impl);
  detector->impl = nullptr;
  detector->sample_rate = 0;
  detector->channels = 0;
  detector->bits_per_sample = 0;
}

}  // extern "C"
