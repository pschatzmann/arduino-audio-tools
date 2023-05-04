#pragma once

#include "AudioCodecs/AudioEncoded.h"

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

  DecoderL8(bool isSigned = true) {
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
    this->active = active;
  }

  /**
   * @brief Construct a new DecoderL8 object
   *
   * @param out_stream Output Stream to which we write the decoded result
   * @param bi Object that will be notified about the Audio Formt (Changes)
   */

  DecoderL8(Print &out_stream, AudioInfoSupport &bi) {
    TRACED();
    setOutputStream(out_stream);
    setNotifyAudioChange(bi);
  }

  /// By default the encoded values are unsigned, but you can change them to signed 
  void setSigned(bool isSigned) { is_signed = isSigned; }

  void begin(AudioInfo info1) {
    TRACED();
    info = info1;
    info.bits_per_sample = 16;  //
    if (p_notify != nullptr) {
      p_notify->setAudioInfo(info);
    }
    active = true;
  }

  void begin() override {
    TRACED();
    active = true;
  }

  void end() override {
    TRACED();
    active = false;
  }

  // for most decoder this is not needed
  virtual void setAudioInfo(AudioInfo from) override {
    TRACED();
    from.bits_per_sample = 16;
    if (info != from) {
      if (p_notify != nullptr) {
        p_notify->setAudioInfo(from);
      }
    }
    info = from;
  }

  virtual size_t write(const void *in_ptr, size_t in_size) override {
    if (p_print == nullptr) return 0;
    buffer.resize(in_size);
    int8_t *pt8 = (int8_t *)in_ptr;
    for (size_t j = 0; j < in_size; j++) {
      int16_t tmp = pt8[j];
      if (!is_signed) {
        tmp -= 128;
      }
      buffer[j] = tmp * 256;
    }
    return p_print->write((uint8_t *)buffer.data(), in_size * sizeof(int16_t));
  }

  virtual operator bool() override { return active; }

 protected:
  bool active;
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
  EncoderL8(bool isSigned = true) {
    TRACED();
    setSigned(isSigned);
  }

  // Constructor providing the output stream
  EncoderL8(Print &out) { p_print = &out; }

  /// By default the encoded values are unsigned, but can change them to signed 
  void setSigned(bool isSigned) { is_signed = isSigned; }

  /// Defines the output Stream
  void setOutputStream(Print &out_stream) override { p_print = &out_stream; }

  /// Provides "audio/pcm"
  const char *mime() override { return "audio/l8"; }

  /// We actually do nothing with this
  virtual void setAudioInfo(AudioInfo from) override {}

  /// starts the processing using the actual RAWAudioInfo
  virtual void begin() override { is_open = true; }

  /// starts the processing
  void begin(Print &out) {
    p_print = &out;
    begin();
  }

  /// stops the processing
  void end() override { is_open = false; }

  /// Writes PCM data to be encoded as RAW
  virtual size_t write(const void *in_ptr, size_t in_size) override {
    if (p_print == nullptr) return 0;
    int16_t *pt16 = (int16_t *)in_ptr;
    buffer.resize(in_size);
    size_t samples = in_size / 2;
    for (size_t j = 0; j < samples; j++) {
      int16_t tmp = pt16[j] / 258;
      if (!is_signed) {
        tmp += 128;
      }
      buffer[j] = tmp;
    }

    return p_print->write((uint8_t *)buffer.data(), samples);
  }

  operator bool() override { return is_open; }

  bool isOpen() { return is_open; }

 protected:
  Print *p_print = nullptr;
  bool is_open;
  bool is_signed = false;
  Vector<int8_t> buffer;
};

}  // namespace audio_tools
