#pragma once

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/AudioCodecs/CodecG7xx.h"

namespace audio_tools {

/**
 * @brief DecoderBasic - supports mime type audio/basic
 * Requires https://github.com/pschatzmann/arduino-libg7xx
 * The content of the "audio/basic" subtype is single channel audio
 * encoded using 8bit ISDN mu-law [PCM] at a sample rate of 8000 Hz.
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class DecoderBasic : public AudioDecoder {
public:
  /**
   * @brief Construct a new DecoderBasic object
   */

  DecoderBasic() { TRACED(); }

  /**
   * @brief Construct a new DecoderBasic object
   *
   * @param out_stream Output Stream to which we write the decoded result
   */
  DecoderBasic(Print &out_stream, bool active = true) {
    TRACED();
    setOutput(out_stream);
  }

  /**
   * @brief Construct a new DecoderBasic object
   *
   * @param out_stream Output Stream to which we write the decoded result
   * @param bi Object that will be notified about the Audio Formt (Changes)
   */

  DecoderBasic(Print &out_stream, AudioInfoSupport &bi) {
    TRACED();
    setOutput(out_stream);
    addNotifyAudioChange(bi);
  }

  /// Defines the output Stream
  void setOutput(Print &out_stream) override {
    decoder.setOutput(out_stream);
  }

  void addNotifyAudioChange(AudioInfoSupport &bi) override {
    decoder.addNotifyAudioChange(bi);
  }

  AudioInfo audioInfo() override { return decoder.audioInfo(); }

  bool begin(AudioInfo info) { 
    decoder.setAudioInfo(info);
    return decoder.begin(); 
  }

  bool begin() override {
    TRACED();
    return decoder.begin();
  }

  void end() override { decoder.end(); }

  virtual size_t write(const uint8_t *data, size_t len) override {
    return decoder.write((uint8_t *)data, len);
  }

  virtual operator bool() override { return decoder; }

protected:
  G711_ULAWDecoder decoder;
};

/**
 * @brief EncoderBasic - supports mime type audio/basic.
 *  The content of the "audio/basic" subtype is single channel audio
 *  encoded using 8bit ISDN mu-law [PCM] at a sample rate of 8000 Hz.
 * Requires https://github.com/pschatzmann/arduino-libg7xx
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class EncoderBasic : public AudioEncoder {
public:
  // Empty Constructor - the output stream must be provided with begin()
  EncoderBasic() {}

  // Constructor providing the output stream
  EncoderBasic(Print &out) { setOutput(out); }

  /// Defines the output Stream
  void setOutput(Print &out) override { encoder.setOutput(out); }

  /// Provides "audio/pcm"
  const char *mime() override { return "audio/basic"; }

  /// We actually do nothing with this
  virtual void setAudioInfo(AudioInfo from) override {
    AudioEncoder::setAudioInfo(from);
    encoder.setAudioInfo(from);
  }

  /// starts the processing using the actual RAWAudioInfo
  bool begin() override { return encoder.begin(); }

  /// stops the processing
  void end() override { encoder.end(); }

  /// Writes PCM data to be encoded as RAW
  virtual size_t write(const uint8_t *in_ptr, size_t in_size) override {
    return encoder.write((uint8_t *)in_ptr, in_size);
  }

  operator bool() override {
    return encoder;
  }

  bool isOpen() { return encoder; }

protected:
  G711_ULAWEncoder encoder;
};

} // namespace audio_tools
