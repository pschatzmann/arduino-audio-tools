#pragma once

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/CoreAudio/AudioBasic/Net.h"
#if defined(ARDUINO) && !defined(IS_MIN_DESKTOP)
#include "Print.h"
#endif

namespace audio_tools {

/**
 * @brief PCM decoder which converts from network format to the host format.
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class DecoderNetworkFormat : public AudioDecoder {
 public:
  DecoderNetworkFormat() = default;

  DecoderNetworkFormat(Print &out_stream) {
    TRACED();
    pt_print = &out_stream;
  }

  DecoderNetworkFormat(Print &out_stream, AudioInfoSupport &bi) {
    pt_print = &out_stream;
  }

  ~DecoderNetworkFormat() {}

  virtual void setOutput(Print &out_stream) { pt_print = &out_stream; }

  bool begin() { return true; }

  void end() {}

  size_t write(const uint8_t *data, size_t len) {
    TRACED();
    switch (audioInfo().bits_per_sample) {
      case 8:
        // nothing to do
        break;
      case 16: {
        int16_t *data16 = (int16_t *)data;
        for (int i = 0; i < len / sizeof(int16_t); i++) {
          data16[i] = ntohs(data16[i]);
        }
      } break;
      case 24:
      case 32: {
        int32_t *data32 = (int32_t *)data;
        for (int i = 0; i < len / sizeof(int32_t); i++) {
          data32[i] = ntohl(data32[i]);
        }
      } break;
      default:
        LOGE("bits_per_sample not supported: %d",
             (int)audioInfo().bits_per_sample);
        break;
    }
    return pt_print->write((uint8_t *)data, len);
  }

  operator bool() { return true; }

  /// The result is encoded data - by default this is false
  virtual bool isResultPCM() { return true; }

 protected:
  Print *pt_print = nullptr;
};

/**
 * @brief  Encoder which converts from the host format to the network format.
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class EncoderNetworkFormat : public AudioEncoder {
 public:
  EncoderNetworkFormat() { TRACED(); }

  EncoderNetworkFormat(Print &out_stream) {
    TRACED();
    pt_print = &out_stream;
  }

  EncoderNetworkFormat(Print &out_stream, AudioInfoSupport &bi) {
    pt_print = &out_stream;
  }

  ~EncoderNetworkFormat() {}

  virtual void setOutput(Print &out_stream) { pt_print = &out_stream; }

  bool begin() { return true; }

  void end() {}

  size_t write(const uint8_t *data, size_t len) {
    TRACED();
    switch (audioInfo().bits_per_sample) {
      case 8:
        // nothing to do
        break;
      case 16: {
        int16_t *data16 = (int16_t *)data;
        for (int i = 0; i < len / sizeof(int16_t); i++) {
          data16[i] = htons(data16[i]);
        }
      } break;
      case 24:
      case 32: {
        int32_t *data32 = (int32_t *)data;
        for (int i = 0; i < len / sizeof(int32_t); i++) {
          data32[i] = htonl(data32[i]);
        }
      } break;
      default:
        LOGE("bits_per_sample not supported: %d",
             (int)audioInfo().bits_per_sample);
        break;
    }
    return pt_print->write((uint8_t *)data, len);
  }

  operator bool() { return true; }

  const char *mime() { return "audio/pcm"; }

 protected:
  Print *pt_print = nullptr;
};

}  // namespace audio_tools
