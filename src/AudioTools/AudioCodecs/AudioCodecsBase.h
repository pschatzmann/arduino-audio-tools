#pragma once

#include "AudioToolsConfig.h"
#include "AudioLogger.h"
#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/CoreAudio/AudioTypes.h"
#include "AudioTools/CoreAudio/BaseStream.h"
#include "AudioTools/CoreAudio/AudioOutput.h"

namespace audio_tools {

/**
 * @brief Decoding of encoded audio into PCM data
 * @ingroup codecs
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioDecoder : public AudioWriter, public AudioInfoSource {
 public:
  AudioDecoder() = default;
  virtual ~AudioDecoder() = default;
  AudioDecoder(AudioDecoder const &) = delete;
  AudioDecoder &operator=(AudioDecoder const &) = delete;

  AudioInfo audioInfo() override { return info; };

  /// for most decoders this is not needed
  void setAudioInfo(AudioInfo from) override {
    TRACED();
    if (info != from) {
      info = from;
      notifyAudioChange(from);
    }
  }
  /// Defines where the decoded result is written to
  virtual void setOutput(AudioStream &out_stream) {
    Print *p_print = &out_stream;
    setOutput(*p_print);
    addNotifyAudioChange(out_stream);
  }

  /// Defines where the decoded result is written to
  virtual void setOutput(AudioOutput &out_stream) {
    Print *p_print = &out_stream;
    setOutput(*p_print);
    addNotifyAudioChange(out_stream);
  }

  /// Defines where the decoded result is written to
  virtual void setOutput(Print &out_stream) override { p_print = &out_stream; }

  /// Returns true to indicate that the decoding result is PCM data
  virtual bool isResultPCM() { return true; }
  virtual bool begin(AudioInfo info) override {
    setAudioInfo(info);
    return begin();
  }
  bool begin() override { return true; }
  void end() override {}

  /// custom id to be used by application
  int id;

  Print* getOutput(){
    return p_print;
  }

  /// Some decoders need e.g. a magic cookie to provide the relevant info for decoding
  virtual bool setCodecConfig(const uint8_t* data, size_t len){
    LOGE("not implemented");
    return false;
  }

 protected:
  Print *p_print = nullptr;
  AudioInfo info;
};

/**
 * @brief Parent class for all container formats
 * @ingroup codecs
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class ContainerDecoder : public AudioDecoder {
  bool isResultPCM() override { return true; }
};


/**
 * @brief  Encoding of PCM data
 * @ingroup codecs
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioEncoder : public AudioWriter {
 public:
  AudioEncoder() = default;
  virtual ~AudioEncoder() = default;
  AudioEncoder(AudioEncoder const &) = delete;
  AudioEncoder &operator=(AudioEncoder const &) = delete;
  /// Provides the mime type of the encoded result
  virtual const char *mime() = 0;
  /// Defines the sample rate, number of channels and bits per sample
  void setAudioInfo(AudioInfo from) override { info = from; }
  AudioInfo audioInfo() override { return info; }
  /// Default output assignment (encoders may override to store Print reference)
  virtual void setOutput(Print &out_stream) override { (void)out_stream; }
  /// Optional rtsp function: provide the frame duration in microseconds
  virtual uint32_t frameDurationUs() { return 0;};
  /// Optional rtsp function: provide samples per the frame
  virtual uint16_t samplesPerFrame() { return 0;};

 protected:
  AudioInfo info;
};

class AudioDecoderExt : public AudioDecoder {
 public:
  virtual void setBlockSize(int blockSize) = 0;
};

class AudioEncoderExt : public AudioEncoder {
 public:
  virtual int blockSize() = 0;
};

/**
 * @brief Dummy no implmentation Codec. This is used so that we can initialize
 * some pointers to decoders and encoders to make sure that they do not point to
 * null.
 * @ingroup codecs
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class CodecNOP : public AudioDecoder, public AudioEncoder {
 public:
  static CodecNOP *instance() {
    static CodecNOP self;
    return &self;
  }

  virtual bool begin() { return true; }
  virtual void end() {}
  virtual void setOutput(Print &out_stream) {}
  virtual void addNotifyAudioChange(AudioInfoSupport &bi) {}
  virtual void setAudioInfo(AudioInfo info) {}

  virtual AudioInfo audioInfo() {
    AudioInfo info;
    return info;
  }
  virtual operator bool() { return false; }
  virtual int readStream(Stream &in) { return 0; };

  // just output silence
  virtual size_t write(const uint8_t *data, size_t len) {
    memset((void *)data, 0, len);
    return len;
  }

  virtual const char *mime() { return nullptr; }
};

}  // namespace audio_tools