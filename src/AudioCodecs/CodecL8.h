#pragma once

#include "AudioCodecs/AudioCodecsBase.h"

namespace audio_tools {

/**
 * @brief DecoderL8 - Converts an 8 Bit Stream into 16Bits
 * Most microcontrollers can not output 8 bit data directly. 8 bit data however
 * is very memory efficient and helps if you need to store audio on constrained
 * resources. This decoder translates 8bit data into 16bit data.
 * By default the encoded data is represented as uint8_t, so the values are from
 * 0 to 255.
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class DecoderL8 : public AudioDecoder {
 public:
  /**
   * @brief Construct a new DecoderL8 object
   */

  DecoderL8(bool isSigned = false) {
    TRACED();
    setSigned(isSigned);
  }

  /**
   * @brief Construct a new DecoderL8 object
   *
   * @param out_stream Output Stream to which we write the decoded result
   */
  DecoderL8(Print &out_stream, bool active = true) {
    TRACED();
    p_print = &out_stream;
  }

  /**
   * @brief Construct a new DecoderL8 object
   *
   * @param out_stream Output Stream to which we write the decoded result
   * @param bi Object that will be notified about the Audio Formt (Changes)
   */

  DecoderL8(Print &out_stream, AudioInfoSupport &bi) {
    TRACED();
    setOutput(out_stream);
    addNotifyAudioChange(bi);
  }

  /// By default the encoded values are unsigned, but you can change them to
  /// signed
  void setSigned(bool isSigned) { is_signed = isSigned; }

  /// for most decoders this is not needed
  virtual void setAudioInfo(AudioInfo from) override {
    TRACED();
    if (from.bits_per_sample!=16){
      LOGE("Bits per sample not supported: %d", from.bits_per_sample);
    }
    from.bits_per_sample = 16;
    if (info != from) {
      notifyAudioChange(from);
    }
    info = from;
  }

  virtual size_t write(const uint8_t *data, size_t len) override {
    if (p_print == nullptr) return 0;
    buffer.resize(len);
    memset(buffer.data(), 0, len * 2);
    if (is_signed) {
      int8_t *pt8 = (int8_t *)data;
      for (size_t j = 0; j < len; j++) {
        buffer[j] = convertSample(pt8[j]);
      }
    } else {
      uint8_t *pt8 = (uint8_t *)data;
      for (size_t j = 0; j < len; j++) {
        buffer[j] = convertSample(pt8[j]);
      }
    }
    int write_byte_count = len * sizeof(int16_t);
    size_t result = p_print->write((uint8_t *)buffer.data(), write_byte_count);
    LOGD("DecoderL8 %d -> %d -> %d", (int)len, write_byte_count, (int)result);
    return result / sizeof(int16_t);
  }

  int16_t convertSample(int16_t in) {
    int32_t tmp = in;
    if (!is_signed) {
      tmp -= 129;
    }
    return NumberConverter::clipT<int32_t, int16_t>(tmp * 258);
  }

  virtual operator bool() override { return p_print!=nullptr; }


 protected:
  bool is_signed = false;
  Vector<int16_t> buffer;
};

/**
 * @brief EncoderL8s - Condenses 16 bit PCM data stream to 8 bits
 * data.
 * Most microcontrollers can not process 8 bit audio data directly. 8 bit data
 * however is very memory efficient and helps if you need to store audio on
 * constrained resources. This encoder translates 16bit data into 8bit data.
 * By default the encoded data is represented as uint8_t, so the values are from
 * 0 to 255.
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class EncoderL8 : public AudioEncoder {
 public:
  // Empty Constructor - the output stream must be provided with begin()
  EncoderL8(bool isSigned = false) {
    TRACED();
    setSigned(isSigned);
  }

  // Constructor providing the output stream
  EncoderL8(Print &out) { p_print = &out; }

  /// By default the encoded values are unsigned, but can change them to signed
  void setSigned(bool isSigned) { is_signed = isSigned; }

  /// Defines the output Stream
  void setOutput(Print &out_stream) override { p_print = &out_stream; }

  /// Provides "audio/pcm"
  const char *mime() override { return "audio/l8"; }

  /// starts the processing using the actual RAWAudioInfo
  bool begin() override { is_open = true; return true;}

  /// starts the processing
  bool begin(Print &out) {
    p_print = &out;
    return begin();
  }

  /// stops the processing
  void end() override { is_open = false; }

  /// Writes PCM data to be encoded as RAW
  size_t write(const uint8_t *data, size_t len) override {
    if (p_print == nullptr) return 0;
    int16_t *pt16 = (int16_t *)data;
    size_t samples = len / 2;
    buffer.resize(samples);
    memset(buffer.data(), 0, samples);
    for (size_t j = 0; j < samples; j++) {
      buffer[j] = convertSample(pt16[j]);
    }

    size_t result = p_print->write((uint8_t *)buffer.data(), samples);
    LOGD("EncoderL8 %d -> %d -> %d", (int)len,(int) samples, (int)result);
    return result * sizeof(int16_t);
  }

  operator bool() override { return is_open; }

  int16_t convertSample(int16_t sample) {
    int16_t tmp = NumberConverter::clipT<int16_t, int8_t>(sample / 258);
    if (!is_signed) {
      tmp += 129;
      // clip to range
      if (tmp < 0) {
        tmp = 0;
      } else if (tmp > 255) {
        tmp = 255;
      }
    }
    return tmp;
  }

  bool isOpen() { return is_open; }

 protected:
  Print *p_print = nullptr;
  bool is_open;
  bool is_signed = false;
  Vector<int8_t> buffer;
};

}  // namespace audio_tools
