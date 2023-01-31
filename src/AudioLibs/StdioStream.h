#pragma once
#include <unistd.h>
#include "AudioTools/AudioStreams.h"

/**
 * @brief Direct binary Audio Output to stdout. On linux you can hear the audio e.g. with ./generator | aplay -f cd
 * @author Phil Schatzmann
 * @ingroup io
 * @copyright GPLv3
 */
namespace audio_tools {
class StdioStream : public AudioStreamX {
public:
  AudioBaseInfo defaultConfig() {
    AudioBaseInfo def;
    def.bits_per_sample = 16;
    def.sample_rate = 44100;
    def.channels = 2;
    return def;
  }

  bool begin(AudioBaseInfo info) {
    is_open = true;
    return true;
  }
  int available() override { return DEFAULT_BUFFER_SIZE; }

  size_t readBytes(uint8_t* buffer, size_t len){
    // read from stdin
    return ::read(0, buffer, len);
  }

  int availableForWrite() override { return DEFAULT_BUFFER_SIZE; }

  size_t write(const uint8_t *buffer, size_t len) override {
    if (!is_open)
      return 0;
    // write to stdout
    return ::write(1, buffer, len);
  }

  void end() {
    is_open = false;
  }

protected:
  bool is_open = false;
  FILE *out;
};

} // namespace audio_tools