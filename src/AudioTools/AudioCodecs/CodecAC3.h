#pragma once

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AC3Decoder.h"

namespace audio_tools {

/**
 * @brief AC-3 (Dolby Digital / ATSC A/52) Decoder using
 * https://github.com/pschatzmann/codec-ac3
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AC3AudioDecoder : public AudioDecoder {
 public:
  AC3AudioDecoder() = default;

  const char *mime() override { return "audio/ac3"; }

  bool begin() override {
    TRACEI();
    is_active = ac3.begin();
    if (is_active) {
      ac3.setPCMCallback(pcmCallback, this);
      if (output_channels != 0) ac3.setOutputChannels(output_channels);
    }
    return is_active;
  }

  void end() override {
    TRACEI();
    ac3.end();
    is_active = false;
  }

  size_t write(const uint8_t *data, size_t len) override {
    if (!is_active) return 0;
    return ac3.write(data, len);
  }

  operator bool() override { return is_active; }

  /// Requests downmixing to n output channels (1 = mono, 2 = stereo)
  /// whenever the source has more channels than that; 0 (the default)
  /// always outputs the source's discrete channel set.
  void setOutputChannels(int n) {
    output_channels = n;
    ac3.setOutputChannels(n);
  }

 protected:
  ::AC3Decoder ac3;
  bool is_active = false;
  int output_channels = 0;

  static void pcmCallback(const int16_t *pcm, int frameCount, int channels,
                           int sampleRate, void *ref) {
    auto *self = (AC3AudioDecoder *)ref;
    self->onPcm(pcm, frameCount, channels, sampleRate);
  }

  void onPcm(const int16_t *pcm, int frameCount, int channels,
             int sampleRate) {
    if (info.channels != channels || info.sample_rate != sampleRate) {
      info.channels = channels;
      info.sample_rate = sampleRate;
      info.bits_per_sample = 16;
      notifyAudioChange(info);
    }
    if (p_print == nullptr) return;
    writeBlocking(p_print, (uint8_t *)pcm,
                  frameCount * channels * sizeof(int16_t));
  }
};

}  // namespace audio_tools
