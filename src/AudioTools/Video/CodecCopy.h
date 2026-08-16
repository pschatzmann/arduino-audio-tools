#pragma once
#include "AudioTools/AudioCodecs/AudioCodecsBase.h"

namespace audio_tools {

class CodecCopy {
 public:
  CodecCopy(AudioWriter& to, Stream& from, int len = 1024)
      : p_out(&to), p_in(&from), buffer_len(len) {}

  size_t copy() {
    uint8_t buffer[buffer_len];
    size_t result = p_in->readBytes(buffer, buffer_len);
    return p_out->write(buffer, result);
  }

  size_t copy(int len) {
    uint8_t buffer[buffer_len];
    size_t total = 0;
    while ((int)total < len) {
      size_t chunk = min((size_t)(len - total), (size_t)buffer_len);
      size_t read = p_in->readBytes(buffer, chunk);
      if (read == 0) break;
      total += p_out->write(buffer, read);
    }
    return total;
  }

 protected:
  AudioWriter* p_out = nullptr;
  Stream* p_in = nullptr;
  int buffer_len = 1024;
};

}  // namespace audio_tools