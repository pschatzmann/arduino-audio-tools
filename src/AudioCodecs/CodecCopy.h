#pragma once

#include "AudioCodecs/AudioEncoded.h"
#include "Stream.h"

namespace audio_tools {

/**
 * @brief Dummy Decoder which just copies the provided data to the output
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class CodecCopy : public AudioDecoder {
public:
  CodecCopy() { LOGD(LOG_METHOD); }

  CodecCopy(Print &out_stream) { LOGD(LOG_METHOD); pt_print=&out_stream; }

  CodecCopy(Print &out_stream, AudioBaseInfoDependent &bi) {pt_print=&out_stream;}

  ~CodecCopy() {}

  virtual void setOutputStream(Print &out_stream) {pt_print=&out_stream;}

  void begin() {}

  void end() {}

  AudioBaseInfo audioInfo() { AudioBaseInfo dummy; return dummy; }

  size_t write(const void *data, size_t len) { return pt_print->write((uint8_t*)data,len); }

  operator bool() { return true; }

  void setNotifyAudioChange(AudioBaseInfoDependent &bi) {}

protected:
  Print *pt_print=nullptr;
};

} // namespace audio_tools

