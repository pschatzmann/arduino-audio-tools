#pragma once

#include "AudioCodecs/AudioEncoded.h"
#include "Stream.h"

namespace audio_tools {

/**
 * @brief Dummy Decoder which just copies the provided data to the output
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class CopyDecoder : public AudioDecoder {
public:
  CopyDecoder() { LOGD(LOG_METHOD); }

  CopyDecoder(Print &out_stream) { LOGD(LOG_METHOD); pt_print=&out_stream; }

  CopyDecoder(Print &out_stream, AudioBaseInfoDependent &bi) {pt_print=&out_stream;}

  ~CopyDecoder() {}

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

/**
 * @brief Dummy Encoder which just copies the provided data to the output
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class CopyEncoder : public AudioEncoder {
public:
  CopyEncoder() { LOGD(LOG_METHOD); }

  CopyEncoder(Print &out_stream) { LOGD(LOG_METHOD); pt_print=&out_stream; }

  CopyEncoder(Print &out_stream, AudioBaseInfoDependent &bi) {pt_print=&out_stream;}

  ~CopyEncoder() {}

  virtual void setOutputStream(Print &out_stream) {pt_print=&out_stream;}

  void begin() {}

  void end() {}

  AudioBaseInfo audioInfo() { return info; }
  void setAudioInfo(AudioBaseInfo ai) { info = ai; }

  size_t write(const void *data, size_t len) { return pt_print->write((uint8_t*)data,len); }

  operator bool() { return true; }

  void setNotifyAudioChange(AudioBaseInfoDependent &bi) {}

  const char *mime() {return nullptr;}


protected:
  Print *pt_print=nullptr;
  AudioBaseInfo info;
};


} // namespace audio_tools

