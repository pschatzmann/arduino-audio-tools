#pragma once

#include "AudioCodecs/AudioCodecsBase.h"
#if defined(ARDUINO) && !defined(IS_MIN_DESKTOP)
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

  bool begin() { return true; }

  void end() {}

  size_t write(const uint8_t *data, size_t len) { 
    TRACED();
    return pt_print->write((uint8_t*)data,len);
  }

  operator bool() { return true; }

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

  bool begin() { return true;}

  void end() {}

  size_t write(const uint8_t *data, size_t len) { return pt_print->write((uint8_t*)data,len); }

  operator bool() { return true; }

  const char *mime() {return "audio/pcm";}


protected:
  Print *pt_print=nullptr;
};

using PCMEncoder = CopyEncoder;
using PCMDecoder = CopyDecoder;

} // namespace audio_tools

