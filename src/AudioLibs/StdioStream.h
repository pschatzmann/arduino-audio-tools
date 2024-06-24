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
class StdioStream : public BaseStream {
public:
  AudioInfo defaultConfig() {
    AudioInfo def;
    def.bits_per_sample = 16;
    def.sample_rate = 44100;
    def.channels = 2;
    return def;
  }

  bool begin()  {
    is_open = true;
    return true;
  }


  int available() override { return DEFAULT_BUFFER_SIZE; }

  size_t readBytes(uint8_t* data, size_t len) override {
    // read from stdin
    return ::read(0, data, len);
  }

  int availableForWrite() override { return DEFAULT_BUFFER_SIZE; }

  size_t write(const uint8_t *data, size_t len) override {
    if (!is_open)
      return 0;
    // write to stdout
    return ::write(1, data, len);
  }

  void end()  {
    is_open = false;
  }

protected:
  bool is_open = false;
};

} // namespace audio_tools