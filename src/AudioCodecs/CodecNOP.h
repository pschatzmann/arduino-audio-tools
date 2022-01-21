#pragma once

#include "AudioTools/AudioTypes.h"
#include "Stream.h"

namespace audio_tools {

/**
 * @brief Dummy Decoder which does nothing
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class DecoderNOP : public AudioDecoder {
public:
  DecoderNOP() { LOGD(LOG_METHOD); }

  DecoderNOP(Print &out_stream) { LOGD(LOG_METHOD); }

  DecoderNOP(Print &out_stream, AudioBaseInfoDependent &bi) {}

  ~DecoderNOP() {}

  virtual void setOutputStream(Print &outStream) {}

  void begin() {}

  void end() {}

  AudioBaseInfo audioInfo() { AudioBaseInfo dummy; return dummy; }

  size_t write(const void *data, size_t len) { return len; }

  operator boolean() { return true; }

  void setNotifyAudioChange(AudioBaseInfoDependent &bi) {}
};

} // namespace audio_tools

