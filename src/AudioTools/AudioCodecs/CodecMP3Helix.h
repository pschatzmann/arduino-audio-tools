#pragma once

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#ifndef HELIX_PRINT
#define HELIX_PRINT
#endif
#include "MP3DecoderHelix.h"

namespace audio_tools {

/**
 * @brief MP3 Decoder using libhelix:
 * https://github.com/pschatzmann/arduino-libhelix This is basically just a
 * simple wrapper to provide AudioInfo and AudioInfoSupport
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MP3DecoderHelix : public AudioDecoder {
 public:
  MP3DecoderHelix() {
    TRACED();
    mp3 = new libhelix::MP3DecoderHelix();
    if (mp3 != nullptr) {
      mp3->setReference(this);
    } else {
      LOGE("Not enough memory for libhelix");
    }
  }
  /**
   * @brief Construct a new MP3DecoderMini object
   *
   * @param out_stream
   */
  MP3DecoderHelix(Print &out_stream) {
    TRACED();
    mp3 = new libhelix::MP3DecoderHelix();
    if (mp3 != nullptr) {
      mp3->setReference(this);
    } else {
      LOGE("Not enough memory for libhelix");
    }
    setOutput(out_stream);
  }

  /**
   * @brief Construct a new MP3DecoderMini object. The decoded output will go to
   * the print object.
   *
   * @param out_stream
   * @param bi
   */
  MP3DecoderHelix(Print &out_stream, AudioInfoSupport &bi) {
    TRACED();
    mp3 = new libhelix::MP3DecoderHelix();
    if (mp3 != nullptr) {
      mp3->setReference(this);
    } else {
      LOGE("Not enough memory for libhelix");
    }
    setOutput(out_stream);
    addNotifyAudioChange(bi);
  }

  /**
   * @brief Destroy the MP3DecoderMini object
   *
   */
  ~MP3DecoderHelix() {
    if (mp3 != nullptr) delete mp3;
  }

  /// Defines the output Stream
  void setOutput(Print &outStream) override {
    AudioDecoder::setOutput(outStream);
    if (mp3 != nullptr) mp3->setOutput(outStream);
  }

  /// Starts the processing
  bool begin() override {
    TRACEI();
    if (mp3 == nullptr) {
      LOGE("Not enough memory for libhelix");
      return false;
    }
    mp3->begin();
    return true;
  }

  /// Releases the reserved memory
  void end() override {
    TRACED();
    if (mp3 != nullptr) mp3->end();
  }

  MP3FrameInfo audioInfoEx() { return mp3->audioInfo(); }

  AudioInfo audioInfo() override {
    AudioInfo baseInfo;
    MP3FrameInfo i = audioInfoEx();
    if (i.nChans != 0 && i.samprate != 0 && i.bitsPerSample != 0) {
      baseInfo.channels = i.nChans;
      baseInfo.sample_rate = i.samprate;
      baseInfo.bits_per_sample = i.bitsPerSample;
    }
    return baseInfo;
  }

  /// Write mp3 data to decoder
  size_t write(const uint8_t *data, size_t len) override {
    LOGD("%s: %zu", LOG_METHOD, len);
    if (mp3 == nullptr) return 0;
    return mp3->write((uint8_t *)data, len);
  }

  /// checks if the class is active
  operator bool() override { return mp3 != nullptr && (bool)*mp3; }

  libhelix::MP3DecoderHelix *driver() { return mp3; }

  /// Defines the callback object to which the Audio information change is
  /// provided
  void addNotifyAudioChange(AudioInfoSupport &bi) override {
    TRACED();
    AudioDecoder::addNotifyAudioChange(bi);
    if (mp3 != nullptr) mp3->setInfoCallback(infoCallback, this);
  }

  /// notifies the subscriber about a change
  static void infoCallback(MP3FrameInfo &i, void *ref) {
    MP3DecoderHelix *p_helix = (MP3DecoderHelix *)ref;
    if (p_helix != nullptr) {
      TRACED();
      AudioInfo baseInfo;
      baseInfo.channels = i.nChans;
      baseInfo.sample_rate = i.samprate;
      baseInfo.bits_per_sample = i.bitsPerSample;
      baseInfo.logInfo("MP3DecoderHelix");
      p_helix->notifyAudioChange(baseInfo);
    } else {
      LOGE("Wrong Libhelix Version");
    }
  }

  /// Provides the maximum frame size - this is allocated on the heap and you
  /// can reduce the heap size my minimizing this value
  size_t maxFrameSize() { return mp3->maxFrameSize(); }

  /// Define your optimized maximum frame size
  void setMaxFrameSize(size_t len) { mp3->setMaxFrameSize(len); }

  /// Provides the maximum pwm buffer size - this is allocated on the heap and
  /// you can reduce the heap size my minimizing this value
  size_t maxPCMSize() { return mp3->maxPCMSize(); }

  /// Define your optimized maximum pwm buffer size
  void setMaxPCMSize(size_t len) { mp3->setMaxPCMSize(len); }

 protected:
  libhelix::MP3DecoderHelix *mp3 = nullptr;
};

}  // namespace audio_tools
