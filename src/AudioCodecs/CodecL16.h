#pragma once

#include "AudioCodecs/AudioEncoded.h"

namespace audio_tools {

/**
 * @brief DecoderL16 - Converts an 16 Bit Stream into 16Bits network byte order.
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class DecoderL16 : public AudioDecoder {
public:
  /**
   * @brief Construct a new DecoderL16 object
   */

  DecoderL16() { TRACED(); }

  /**
   * @brief Construct a new DecoderL16 object
   *
   * @param out_stream Output Stream to which we write the decoded result
   */
  DecoderL16(Print &out_stream, bool active = true) {
    TRACED();
    p_print = &out_stream;
    this->active = active;
  }

  /**
   * @brief Construct a new DecoderL16 object
   *
   * @param out_stream Output Stream to which we write the decoded result
   * @param bi Object that will be notified about the Audio Formt (Changes)
   */

  DecoderL16(Print &out_stream, AudioInfoSupport &bi) {
    TRACED();
    setOutput(out_stream);
    setNotifyAudioChange(bi);
  }

  /// Defines the output Stream
  void setOutput(Print &out_stream) override { p_print = &out_stream; }

  void setNotifyAudioChange(AudioInfoSupport &bi) override { this->bid = &bi; }

  AudioInfo audioInfo() override { return cfg; }

  void begin(AudioInfo info) {
    TRACED();
    cfg = info;
    if (bid != nullptr) {
      bid->setAudioInfo(cfg);
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

  virtual size_t write(const void *in_ptr, size_t in_size) override {
    if (p_print == nullptr)
      return 0;
    int16_t *data16 = (int16_t *)in_ptr;
    for (int j = 0; j < in_size / 2; j++) {
      data16[j] = ntohs(data16[j]);
    }
    return p_print->write((uint8_t *)in_ptr, in_size);
  }

  virtual operator bool() override { return active; }

protected:
  Print *p_print = nullptr;
  AudioInfoSupport *bid = nullptr;
  AudioInfo cfg;
  bool active;
};

/**
 * @brief EncoderL16s - Condenses 16 bit PCM data stream to 8 bits
 * data.
 * Most microcontrollers can not process 8 bit audio data directly. 8 bit data
 * however is very memory efficient and helps if you need to store audio on
 * constrained resources. This encoder translates 16bit data into 8bit data.
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class EncoderL16 : public AudioEncoder {
public:
  // Empty Constructor - the output stream must be provided with begin()
  EncoderL16() {}

  // Constructor providing the output stream
  EncoderL16(Print &out) { p_print = &out; }

  /// Defines the output Stream
  void setOutput(Print &out_stream) override { p_print = &out_stream; }

  /// Provides "audio/pcm"
  const char *mime() override { return "audio/l16"; }

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
    if (p_print == nullptr)
      return 0;

    int16_t *data16 = (int16_t *)in_ptr;
    for (int j = 0; j < in_size / 2; j++) {
      data16[j] = htons(data16[j]);
    }

    return p_print->write((uint8_t *)in_ptr, in_size);
  }

  operator bool() override { return is_open; }

  bool isOpen() { return is_open; }

protected:
  Print *p_print = nullptr;
  bool is_open;
};

} // namespace audio_tools
