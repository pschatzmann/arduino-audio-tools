#pragma once

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"

namespace audio_tools {
/**
 * @brief Factory for creating new decoders based on the mime type or id
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 */
class CodecFactory {
 public:
  bool addDecoder(const char* id, AudioDecoder* (*cb)()) {
    if (id == nullptr || cb == nullptr) return false;
    DecoderFactoryLine line;
    line.id = id;
    line.cb = cb;
    decoders.push_back(line);
    return true;
  }

  bool addEncoder(const char* id, AudioEncoder* (*cb)()) {
    if (id == nullptr || cb == nullptr) return false;
    EncoderFactoryLine line;
    line.id = id;
    line.cb = cb;
    encoders.push_back(line);
    return true;
  }

  /// create a new decoder instance
  AudioDecoder* createDecoder(const char* str) {
    for (auto& line : decoders) {
      if (line.id.equals(str)) {
        return line.cb();
      }
    }
    return nullptr;
  }
  /// create a new encoder instance
  AudioEncoder* createEncoder(const char* str) {
    for (auto& line : encoders) {
      if (line.id.equals(str)) {
        return line.cb();
      }
    }
    return nullptr;
  }

 protected:
  struct DecoderFactoryLine {
    Str id;
    AudioDecoder* (*cb)() = nullptr;
  };
  struct EncoderFactoryLine {
    Str id;
    AudioEncoder* (*cb)() = nullptr;
  };
  Vector<DecoderFactoryLine> decoders;
  Vector<EncoderFactoryLine> encoders;
};

}  // namespace audio_tools
