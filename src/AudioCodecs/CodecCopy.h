#pragma once

#include "AudioCodecs/AudioEncoded.h"
#ifdef ARDUINO
#include "Print.h"
#endif

namespace audio_tools {

/**
 * @brief Dummy Decoder which just copies the provided data to the output
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class CopyDecoder : public AudioDecoder {
public:
  CopyDecoder() { TRACED(); }

  CopyDecoder(bool isPcm){
    is_pcm = isPcm;
  }

  CopyDecoder(Print &out_stream) { TRACED(); pt_print=&out_stream; }

  CopyDecoder(Print &out_stream, AudioInfoSupport &bi) {pt_print=&out_stream;}

  ~CopyDecoder() {}

  virtual void setOutput(Print &out_stream) {pt_print=&out_stream;}

  void begin() {}

  void end() {}

  AudioInfo audioInfo() { AudioInfo dummy; return dummy; }

  size_t write(const void *data, size_t len) { 
    TRACED();
    return pt_print->write((uint8_t*)data,len);
  }

  operator bool() { return true; }

  void setNotifyAudioChange(AudioInfoSupport &bi) {}

  // The result is encoded data
  virtual bool isResultPCM() { return is_pcm;} 

protected:
  Print *pt_print=nullptr;
  bool is_pcm = false;
};

/**
 * @brief Dummy Encoder which just copies the provided data to the output
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class CopyEncoder : public AudioEncoder {
public:
  CopyEncoder() { TRACED(); }

  CopyEncoder(Print &out_stream) { TRACED(); pt_print=&out_stream; }

  CopyEncoder(Print &out_stream, AudioInfoSupport &bi) {pt_print=&out_stream;}

  ~CopyEncoder() {}

  virtual void setOutput(Print &out_stream) {pt_print=&out_stream;}

  void begin() {}

  void end() {}

  AudioInfo audioInfo() { return info; }
  void setAudioInfo(AudioInfo ai) { info = ai; }

  size_t write(const void *data, size_t len) { return pt_print->write((uint8_t*)data,len); }

  operator bool() { return true; }

  void setNotifyAudioChange(AudioInfoSupport &bi) {}

  const char *mime() {return "audio/pcm";}


protected:
  Print *pt_print=nullptr;
  AudioInfo info;
};

using PCMEncoder = CopyEncoder;
using PCMDecoder = CopyDecoder;

} // namespace audio_tools

