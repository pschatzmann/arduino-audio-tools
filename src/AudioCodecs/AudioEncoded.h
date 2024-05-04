#pragma once

#include "AudioConfig.h"
#include "AudioLogger.h"
#include "AudioTools/AudioIO.h"
#include "AudioTools/AudioOutput.h"
#include "AudioTools/AudioStreams.h"
#include "AudioTools/AudioTypes.h"

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
  /// If true, the decoding result is PCM data
  virtual bool isResultPCM() {
    setAudioInfo(info);
    return begin();
  }
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
  virtual size_t write(const void *in_ptr, size_t in_size) {
    memset((void *)in_ptr, 0, in_size);
    return in_size;
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

#if USE_OBSOLETE
  /// Obsolete: same as setInput
  void setInputStream(Stream &inStream) { setInput(inStream); }
#endif

  /// Provides the last available MP3FrameInfo
  virtual AudioInfo audioInfo() = 0;

  /// checks if the class is active
  virtual operator bool() = 0;

  /// Process a single read operation - to be called in the loop
  virtual bool copy() = 0;

 protected:
  virtual size_t readBytes(uint8_t *buffer, size_t len) = 0;
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

  size_t readBytes(uint8_t *buffer, size_t len) override {
    if (p_input == nullptr) return 0;
    return p_input->readBytes(buffer, len);
  }
};

/**
 * @brief A more natural Print class to process encoded data (aac, wav,
 * mp3...). Just define the output and the decoder and write the encoded
 * data.
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class EncodedAudioOutput : public ModifyingOutput {
 public:
  EncodedAudioOutput() {
    active = false;
  }

  EncodedAudioOutput(AudioDecoder *decoder) {
    setDecoder(decoder);
    active = false;
  }

  EncodedAudioOutput(AudioEncoder *encoder) {
    setEncoder(encoder);
    active = false;
  }

  EncodedAudioOutput(AudioStream *outputStream, AudioDecoder *decoder) {
    setDecoder(decoder);
    setOutput(outputStream);
    active = false;
  }

  EncodedAudioOutput(AudioOutput *outputStream, AudioDecoder *decoder) {
    setDecoder(decoder);
    setOutput(outputStream);
    active = false;
  }

  EncodedAudioOutput(Print *outputStream, AudioDecoder *decoder) {
    setDecoder(decoder);
    setOutput(outputStream);
    active = false;
  }

  EncodedAudioOutput(Print *outputStream, AudioEncoder *encoder) {
    setEncoder(encoder);
    setOutput(outputStream);
    active = false;
  }

  EncodedAudioOutput(AudioOutput *outputStream, AudioEncoder *encoder) {
    setEncoder(encoder);
    setOutput(outputStream);
    active = false;
  }

  EncodedAudioOutput(AudioStream *outputStream, AudioEncoder *encoder) {
    setEncoder(encoder);
    setOutput(outputStream);
    active = false;
  }

  /// Define object which need to be notified if the basinfo is changing
  void addNotifyAudioChange(AudioInfoSupport &bi) override {
    TRACEI();
    static int count = 0;
    count++;
    assert(count<10);
    decoder_ptr->addNotifyAudioChange(bi);
  }

  AudioInfo defaultConfig() {
    AudioInfo cfg;
    cfg.channels = 2;
    cfg.sample_rate = 44100;
    cfg.bits_per_sample = 16;
    return cfg;
  }

  virtual void setAudioInfo(AudioInfo newInfo) override {
    TRACED();
    if (this->cfg != newInfo && newInfo.channels != 0 && newInfo.sample_rate != 0) {
      this->cfg = newInfo;
      decoder_ptr->setAudioInfo(cfg);
      encoder_ptr->setAudioInfo(cfg);
    }
  }

  void setOutput(Print &outputStream) { setOutput(&outputStream); }

  /// Defines the output
  void setOutput(Print *outputStream) {
    ptr_out = outputStream;
    if (decoder_ptr != nullptr) {
      decoder_ptr->setOutput(*ptr_out);
    }
    if (encoder_ptr != nullptr) {
      encoder_ptr->setOutput(*ptr_out);
    }
  }

  void setEncoder(AudioEncoder *encoder) {
    if (encoder == nullptr) {
      encoder = CodecNOP::instance();
    }
    encoder_ptr = encoder;
    writer_ptr = encoder;
    if (ptr_out != nullptr) {
      encoder_ptr->setOutput(*ptr_out);
    }
  }

  AudioEncoder *getEncoder() { return encoder_ptr; }

  void setDecoder(AudioDecoder *decoder) {
    if (decoder == nullptr) {
      decoder = CodecNOP::instance();
    }
    decoder_ptr = decoder;
    writer_ptr = decoder;
    if (ptr_out != nullptr) {
      decoder_ptr->setOutput(*ptr_out);
    }
  }

  AudioDecoder *getDecoder() { return decoder_ptr; }

  /// Starts the processing - sets the status to active
  bool begin() override {
    custom_log_level.set();
    TRACED();
    if (!active) {
      TRACED();
      const CodecNOP *nop = CodecNOP::instance();
      if (decoder_ptr != nop || encoder_ptr != nop) {
        active = true;
        if (!decoder_ptr->begin(cfg)) active = false;
        if (!encoder_ptr->begin(cfg)) active = false;
      } else {
        LOGW("no decoder or encoder defined");
      }
    }
    custom_log_level.reset();
    return active;
  }

  /// Starts the processing - sets the status to active
  virtual bool begin(AudioInfo newInfo) {
    cfg = newInfo;
    return begin();
  }

  /// Ends the processing
  void end() override {
    custom_log_level.set();
    TRACEI();
    decoder_ptr->end();
    encoder_ptr->end();
    active = false;
    custom_log_level.reset();
  }

  /// encoder decode the data
  virtual size_t write(const uint8_t *data, size_t len) override {
    if (len == 0) {
      //LOGI("write: %d", 0);
      return 0;
    }
    custom_log_level.set();
    LOGD("EncodedAudioOutput::write: %d", (int)len);

    if (writer_ptr == nullptr || data == nullptr) {
      LOGE("NPE");
      return 0;
    }

    if (check_available_for_write && availableForWrite() == 0) {
      return 0;
    }

    size_t result = writer_ptr->write(data, len);
    LOGD("EncodedAudioOutput::write: %d -> %d", (int)len, (int)result);
    custom_log_level.reset();

    return result;
  }

  int availableForWrite() override {
    if (!check_available_for_write) return frame_size;
    return min(ptr_out->availableForWrite(), frame_size);
  }

  /// Returns true if status is active and we still have data to be processed
  operator bool() { return active; }

  /// Provides the initialized decoder
  AudioDecoder &decoder() { return *decoder_ptr; }

  /// Provides the initialized encoder
  AudioEncoder &encoder() { return *encoder_ptr; }

  /// Defines the class specific custom log level
  void setLogLevel(AudioLogger::LogLevel level) { custom_log_level.set(level); }
  /// Is Available for Write check activated ?
  bool isCheckAvailableForWrite() { return check_available_for_write; }

  /// defines the size of the decoded frame in bytes
  void setFrameSize(int size) { frame_size = size; }

 protected:
  // AudioInfo info;
  AudioDecoder *decoder_ptr = CodecNOP::instance();  // decoder
  AudioEncoder *encoder_ptr = CodecNOP::instance();  // decoder
  AudioWriter *writer_ptr = nullptr;
  Print *ptr_out = nullptr;
  bool active = false;
  bool check_available_for_write = false;
  CustomLogLevel custom_log_level;
  int frame_size = DEFAULT_BUFFER_SIZE;
};

// legacy name
using EncodedAudioPrint = EncodedAudioOutput;

/**
 * @brief A more natural Stream class to process encoded data (aac, wav,
 * mp3...) which also supports the decoding by calling readBytes().
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class EncodedAudioStream : public ReformatBaseStream {
 public:
  EncodedAudioStream() = default;

  EncodedAudioStream(AudioStream *ioStream, AudioDecoder *decoder) {
    setDecoder(decoder);
    setStream(*ioStream);
  }

  EncodedAudioStream(Stream *ioStream, AudioDecoder *decoder) {
    setDecoder(decoder);
    setStream(*ioStream);
  }

  EncodedAudioStream(AudioOutput *outputStream, AudioDecoder *decoder) {
    setOutput(*outputStream);
    setDecoder(decoder);
  }

  EncodedAudioStream(Print *outputStream, AudioDecoder *decoder) {
    setOutput(*outputStream);
    setDecoder(decoder);
  }

  EncodedAudioStream(Print *outputStream, AudioEncoder *encoder) {
    setOutput(*outputStream);
    setEncoder(encoder);
  }

  EncodedAudioStream(AudioDecoder *decoder) { setDecoder(decoder); }

  EncodedAudioStream(AudioEncoder *encoder) { setEncoder(encoder); }

  void setEncoder(AudioEncoder *encoder) { enc_out.setEncoder(encoder); }

  void setDecoder(AudioDecoder *decoder) { enc_out.setDecoder(decoder); }

  AudioEncoder *getEncoder() { return enc_out.getEncoder(); }

  AudioDecoder *getDecoder() { return enc_out.getDecoder(); }

  /// Provides the initialized decoder
  AudioDecoder &decoder() { return *getDecoder(); }

  /// Provides the initialized encoder
  AudioEncoder &encoder() { return *getEncoder(); }

  void setStream(Stream *stream) {
    setStream(*stream);
  }

  void setStream(AudioStream *stream) {
    setStream(*stream);
  }

  void setOutput(AudioOutput *stream) {
    setOutput(*stream);
  }

  void setOutput(Print *stream) {
    setOutput(*stream);
  }

  void setStream(AudioStream &stream) {
    ReformatBaseStream::setStream(stream);
    enc_out.setOutput(&stream);
  }

  void setStream(Stream &stream) {
    ReformatBaseStream::setStream(stream);
    enc_out.setOutput(&stream);
  }

  void setOutput(AudioOutput &stream) {
    ReformatBaseStream::setOutput(stream);
    enc_out.setOutput(&stream);
  }

  void setOutput(Print &out) {
    ReformatBaseStream::setOutput(out);
    enc_out.setOutput(&out);
  }

  AudioInfo defaultConfig() {
    AudioInfo ai;
    return ai;
  }

  bool begin(AudioInfo info) {
    setAudioInfo(info);
    return begin();
  }

  bool begin() {
    is_output_notify = false;
    reader.setByteCountFactor(10);
    setupReader();
    ReformatBaseStream::begin();
    return enc_out.begin(audioInfo());
  }

  void end() {
    enc_out.end();
    reader.end();
  }

  int availableForWrite() { return enc_out.availableForWrite(); }

  size_t write(const uint8_t *data, size_t len) {
    addNotifyOnFirstWrite();
    return enc_out.write(data, len);
  }

  size_t readBytes(uint8_t *data, size_t size) {
    return reader.readBytes(data, size);
  }

  void addNotifyAudioChange(AudioInfoSupport &bi) override {
    enc_out.addNotifyAudioChange(bi);
  }

  float getByteFactor() { return 1.0f; }

  /// Defines the class specific custom log level
  void setLogLevel(AudioLogger::LogLevel level) { enc_out.setLogLevel(level); }

 protected:
  EncodedAudioOutput enc_out;

};

/**
 * @brief Adapter class which lets an AudioWriter behave like a Print
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */

class AudioWriterToAudioOutput : public AudioOutputAdapter {
 public:
  void setWriter(AudioWriter *writer) { p_writer = writer; }
  size_t write(const uint8_t *in_ptr, size_t in_size) {
    return p_writer->write(in_ptr, in_size);
  };

 protected:
  AudioWriter *p_writer = nullptr;
};

/**
 * @brief ContainerTarget: forwards requests to both the output and the
 * encoder/decoder and sets up the output chain for Containers. We also
 * manage the proper sequence of the output classes
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class ContainerTarget {
 public:
  virtual bool begin() = 0;
  virtual void end() = 0;
  virtual void setAudioInfo(AudioInfo info) {
    if (this->info != info && info.channels != 0 && info.sample_rate != 0) {
      this->info = info;
      if (p_writer1 != nullptr) p_writer1->setAudioInfo(info);
      if (p_writer2 != nullptr) p_writer2->setAudioInfo(info);
    }
  }
  virtual size_t write(uint8_t *data, size_t size) = 0;

 protected:
  AudioInfo info;
  AudioWriter *p_writer1 = nullptr;
  AudioWriter *p_writer2 = nullptr;
  AudioWriterToAudioOutput print2;
  bool active = false;
};

class ContainerTargetPrint : public ContainerTarget {
 public:
  void setupOutput(AudioWriter *writer1, AudioWriter *writer2, Print &print) {
    p_print = &print;
    p_writer1 = writer1;
    p_writer2 = writer2;
    print2.setWriter(p_writer2);
  }

  void setupOutput(AudioWriter *writer1, Print &print) {
    p_print = &print;
    p_writer1 = writer1;
  }

  virtual bool begin() {
    if (!active) {
      active = true;
      if (p_writer2 != nullptr) {
        p_writer1->setOutput(print2);
        p_writer2->setOutput(*p_print);
        p_writer1->begin();
        p_writer2->begin();
      } else {
        p_writer1->setOutput(*p_print);
        p_writer1->begin();
      }
    }
    return true;
  }
  virtual void end() {
    if (active) {
      if (p_writer1 != nullptr) p_writer1->end();
      if (p_writer2 != nullptr) p_writer2->end();
    }
    active = false;
  }
  virtual size_t write(uint8_t *data, size_t size) {
    TRACED();
    return p_writer1->write(data, size);
  }

 protected:
  Print *p_print = nullptr;
  AudioWriterToAudioOutput print2;
};

}  // namespace audio_tools
