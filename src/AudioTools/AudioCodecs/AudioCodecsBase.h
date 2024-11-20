#pragma once

#include "AudioConfig.h"
#include "AudioLogger.h"
#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/CoreAudio/AudioTypes.h"

namespace audio_tools {

/**
 * @brief Docoding of encoded audio into PCM data
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

  virtual AudioInfo audioInfo() { return info; };

  /// for most decoders this is not needed
  virtual void setAudioInfo(AudioInfo from) override {
    TRACED();
    if (info != from) {
      notifyAudioChange(from);
    }
    info = from;
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
  AudioInfo audioInfo() { return info; }

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

/**
 * @brief A Streaming Decoder where we provide both the input and output
 * as streams.
 * @ingroup codecs
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class StreamingDecoder : public AudioInfoSource {
 public:
  /// Starts the processing
  virtual bool begin() = 0;

  /// Releases the reserved memory
  virtual void end() = 0;

  /// Defines the output Stream
  virtual void setOutput(Print &out_stream) { p_print = &out_stream; }

  /// Defines the output streams and register to be notified
  virtual void setOutput(AudioStream &out_stream) {
    Print *p_print = &out_stream;
    setOutput(*p_print);
    addNotifyAudioChange(out_stream);
  }

  /// Defines the output streams and register to be notified
  virtual void setOutput(AudioOutput &out_stream) {
    Print *p_print = &out_stream;
    setOutput(*p_print);
    addNotifyAudioChange(out_stream);
  }

  /// Stream Interface: Decode directly by taking data from the stream. This is
  /// more efficient then feeding the decoder with write: just call copy() in
  /// the loop
  void setInput(Stream &inStream) { this->p_input = &inStream; }

  /// Provides the last available MP3FrameInfo
  virtual AudioInfo audioInfo() = 0;

  /// checks if the class is active
  virtual operator bool() = 0;

  /// Process a single read operation - to be called in the loop
  virtual bool copy() = 0;

 protected:
  virtual size_t readBytes(uint8_t *data, size_t len) = 0;
  Print *p_print = nullptr;
  Stream *p_input = nullptr;
};

/**
 * @brief Converts any AudioDecoder to a StreamingDecoder
 * @ingroup codecs
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class StreamingDecoderAdapter : public StreamingDecoder {
 public:
  StreamingDecoderAdapter(AudioDecoder &decoder,
                          int copySize = DEFAULT_BUFFER_SIZE) {
    p_decoder = &decoder;
    if (copySize > 0) resize(copySize);
  }
  /// Starts the processing
  bool begin() override { return p_input != nullptr && p_decoder->begin(); }

  /// Releases the reserved memory
  void end() override { p_decoder->end(); }

  /// Defines the output Stream
  void setOutput(Print &out_stream) override {
    p_decoder->setOutput(out_stream);
  }

  /// Provides the last available MP3FrameInfo
  AudioInfo audioInfo() override { return p_decoder->audioInfo(); }

  /// checks if the class is active
  virtual operator bool() { return *p_decoder; }

  /// Process a single read operation - to be called in the loop
  virtual bool copy() {
    int read = readBytes(&buffer[0], buffer.size());
    int written = 0;
    if (read > 0) written = p_decoder->write(&buffer[0], read);
    return written > 0;
  }

  /// Adjust the buffer size: the existing content of the buffer is lost!
  void resize(int bufferSize) { buffer.resize(bufferSize); }

 protected:
  AudioDecoder *p_decoder = nullptr;
  Vector<uint8_t> buffer{0};

  size_t readBytes(uint8_t *data, size_t len) override {
    if (p_input == nullptr) return 0;
    return p_input->readBytes(data, len);
  }
};

}  // namespace audio_tools