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
    return writeAll(buffer, result);
  }

  size_t copy(int len) {
    uint8_t buffer[buffer_len];
    size_t total = 0;
    while ((int)total < len) {
      size_t chunk = min((size_t)(len - total), (size_t)buffer_len);
      size_t read = p_in->readBytes(buffer, chunk);
      if (read == 0) break;
      if (writeAll(buffer, read) != read) break;
      total += read;
    }
    return total;
  }

 protected:
  AudioWriter* p_out = nullptr;
  Stream* p_in = nullptr;
  int buffer_len = 1024;

  /// write() (e.g. DemuxerAVI/DemuxerMP4/DemuxerMPG) may only accept part of
  /// a buffer in one call when its internal parse buffer is momentarily full
  /// - retry with the remainder (parsing that happens inside write() itself
  /// drains it further) instead of silently dropping the unwritten tail,
  /// which desyncs the demuxer from the real byte stream and corrupts
  /// parsing further in. Only bails on a genuine stall (write() returns 0).
  size_t writeAll(uint8_t* data, size_t len) {
    size_t written = 0;
    while (written < len) {
      size_t n = p_out->write(data + written, len - written);
      if (n == 0) break;
      written += n;
    }
    return written;
  }
};

}  // namespace audio_tools