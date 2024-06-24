#pragma once

#include "AudioCodecs/AudioCodecsBase.h"
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
    addNotifyAudioChange(bi);
  }

  /// Defines the output Stream
  void setOutput(Print &out_stream) override { p_print = &out_stream; }

  virtual size_t write(const uint8_t *data, size_t len) override {
    if (p_print == nullptr)
      return 0;
    int16_t *data16 = (int16_t *)data;
    for (int j = 0; j < len / 2; j++) {
      data16[j] = ntohs(data16[j]);
    }
    return p_print->write((uint8_t *)data, len);
  }

  virtual operator bool() override { return p_print!=nullptr; }


protected:
  Print *p_print = nullptr;
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

  /// starts the processing using the actual RAWAudioInfo
  virtual bool begin() override { is_open = true; return true;}

  /// starts the processing
  bool begin(Print &out) {
    p_print = &out;
    return begin();
  }

  /// stops the processing
  void end() override { is_open = false; }

  /// Writes PCM data to be encoded as RAW
  virtual size_t write(const uint8_t *data, size_t len) override {
    if (p_print == nullptr)
      return 0;

    int16_t *data16 = (int16_t *)data;
    for (int j = 0; j < len / 2; j++) {
      data16[j] = htons(data16[j]);
    }

    return p_print->write((uint8_t *)data, len);
  }

  operator bool() override { return is_open; }

  bool isOpen() { return is_open; }

protected:
  Print *p_print = nullptr;
  bool is_open;
};

} // namespace audio_tools
