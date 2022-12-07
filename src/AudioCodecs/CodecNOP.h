#pragma once

#include "AudioCodecs/AudioEncoded.h"

/** 
 * @defgroup codec-nop NOP 
 * @ingroup codecs
 * @brief No operation   
**/

namespace audio_tools {

/**
 * @brief Dummy Decoder which does nothing
 * @ingroup codec-nop
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class DecoderNOP : public AudioDecoder {
public:
  DecoderNOP() { TRACED(); }

  DecoderNOP(Print &out_stream) { TRACED(); }

  DecoderNOP(Print &out_stream, AudioBaseInfoDependent &bi) {}

  ~DecoderNOP() {}

  virtual void setOutputStream(Print &outStream) {}

  void begin() {}

  void end() {}

  AudioBaseInfo audioInfo() { AudioBaseInfo dummy; return dummy; }

  size_t write(const void *data, size_t len) { return len; }

  operator bool() { return true; }

  void setNotifyAudioChange(AudioBaseInfoDependent &bi) {}
};

} // namespace audio_tools

