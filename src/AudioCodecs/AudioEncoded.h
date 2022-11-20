#pragma once

#include "AudioConfig.h"
#include "AudioTools/AudioPrint.h"
#include "AudioTools/AudioStreams.h"
#include "AudioTools/AudioTypes.h"
#include "Stream.h"

namespace audio_tools {

/**
 * @brief Docoding of encoded audio into PCM data
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioDecoder : public AudioWriter, public AudioBaseInfoSource {
public:
  AudioDecoder() = default;
  virtual ~AudioDecoder() = default;
  AudioDecoder(AudioDecoder const&) = delete;
  AudioDecoder& operator=(AudioDecoder const&) = delete;
  
  virtual AudioBaseInfo audioInfo() = 0;
  // for most decoder this is not needed
  virtual void setAudioInfo(AudioBaseInfo from) override {}
  virtual void setOutputStream(AudioStream &out_stream) {
    Print *p_print = &out_stream;
    setOutputStream(*p_print);
    setNotifyAudioChange(out_stream);
  }
  virtual void setOutputStream(AudioPrint &out_stream) {
    Print *p_print = &out_stream;
    setOutputStream(*p_print);
    setNotifyAudioChange(out_stream);
  }
  virtual void setOutputStream(Print &out_stream) override = 0;
  // Th decoding result is PCM data
  virtual bool isResultPCM() { return true;} 
};

/**
 * @brief  Encoding of PCM data
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioEncoder : public AudioWriter {
public:
  AudioEncoder() = default;
  virtual ~AudioEncoder() = default;
  AudioEncoder(AudioEncoder const&) = delete;
  AudioEncoder& operator=(AudioEncoder const&) = delete;
  virtual const char *mime() = 0;
};

/**
 * @brief Dummpy no implmentation Codec. This is used so that we can initialize
 * some pointers to decoders and encoders to make sure that they do not point to
 * null.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class CodecNOP : public AudioDecoder, public AudioEncoder {
public:
  static CodecNOP *instance() {
    static CodecNOP self;
    return &self;
  }

  virtual void begin() {}
  virtual void end() {}
  virtual void setOutputStream(Print &out_stream) {}
  virtual void setNotifyAudioChange(AudioBaseInfoDependent &bi) {}
  virtual void setAudioInfo(AudioBaseInfo info) {}

  virtual AudioBaseInfo audioInfo() {
    AudioBaseInfo info;
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
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class StreamingDecoder {
public:
  /// Starts the processing
  virtual void begin() = 0;

  /// Releases the reserved memory
  virtual void end() = 0;

  /// Defines the output Stream
  virtual void setOutputStream(Print &outStream) = 0;

  /// Register Output Stream to be notified about changes
  virtual void setNotifyAudioChange(AudioBaseInfoDependent &bi) = 0;

  /// Defines the output streams and register to be notified
  virtual void setOutputStream(AudioStream &out_stream) {
    Print *p_print = &out_stream;
    setOutputStream(*p_print);
    setNotifyAudioChange(out_stream);
  }

  /// Defines the output streams and register to be notified
  virtual void setOutputStream(AudioPrint &out_stream) {
    Print *p_print = &out_stream;
    setOutputStream(*p_print);
    setNotifyAudioChange(out_stream);
  }

  /// Defines the input data stream
  virtual void setInputStream(Stream &inStream) = 0;

  /// Provides the last available MP3FrameInfo
  virtual AudioBaseInfo audioInfo() = 0;

  /// checks if the class is active
  virtual operator bool() = 0;

  /// Process a single read operation - to be called in the loop
  virtual bool copy() = 0;

protected:
  virtual size_t readBytes(uint8_t *buffer, size_t len) = 0;
};

/**
 * @brief A more natural Stream class to process encoded data (aac, wav,
 * mp3...).
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class EncodedAudioStream : public AudioPrint {
public:
  /// Constructor for AudioStream with automatic notification of audio changes
  EncodedAudioStream(AudioStream *outputStream, AudioDecoder *decoder) {
    TRACED();
    ptr_out = outputStream;
    decoder_ptr = decoder;
    decoder_ptr->setOutputStream(*outputStream);
    decoder_ptr->setNotifyAudioChange(*outputStream);
    writer_ptr = decoder_ptr;
    active = false;
  }

  /// Constructor for AudioPrint with automatic notification of audio changes
  EncodedAudioStream(AudioPrint *outputStream, AudioDecoder *decoder) {
    TRACED();
    ptr_out = outputStream;
    decoder_ptr = decoder;
    decoder_ptr->setOutputStream(*outputStream);
    decoder_ptr->setNotifyAudioChange(*outputStream);
    writer_ptr = decoder_ptr;
    active = false;
  }


  /**
   * @brief Construct a new Encoded Stream object - used for decoding
   *
   * @param outputStream
   * @param decoder
   */
  EncodedAudioStream(Print &outputStream, AudioDecoder &decoder) {
    TRACED();
    ptr_out = &outputStream;
    decoder_ptr = &decoder;
    decoder_ptr->setOutputStream(outputStream);
    writer_ptr = decoder_ptr;
    active = false;
  }

  /**
   * @brief Construct a new Encoded Audio Stream object - used for decoding
   *
   * @param outputStream
   * @param decoder
   */
  EncodedAudioStream(Print *outputStream, AudioDecoder *decoder) {
    TRACED();
    ptr_out = outputStream;
    decoder_ptr = decoder;
    decoder_ptr->setOutputStream(*outputStream);
    writer_ptr = decoder_ptr;
    active = false;
  }

  /**
   * @brief Construct a new Encoded Audio Stream object - used for encoding
   *
   * @param outputStream
   * @param encoder
   */
  EncodedAudioStream(Print &outputStream, AudioEncoder &encoder) {
    TRACED();
    ptr_out = &outputStream;
    encoder_ptr = &encoder;
    encoder_ptr->setOutputStream(outputStream);
    writer_ptr = encoder_ptr;
    active = false;
  }

  /**
   * @brief Construct a new Encoded Audio Stream object - used for encoding
   *
   * @param outputStream
   * @param encoder
   */
  EncodedAudioStream(Print *outputStream, AudioEncoder *encoder) {
    TRACED();
    ptr_out = outputStream;
    encoder_ptr = encoder;
    encoder_ptr->setOutputStream(*outputStream);
    writer_ptr = encoder_ptr;
    active = false;
  }


  /**
   * @brief Construct a new Encoded Audio Stream object - the Output and
   * Encoder/Decoder needs to be defined with the begin method
   *
   */
  EncodedAudioStream() {
    TRACED();
    active = false;
  }

  /**
   * @brief Destroy the Encoded Audio Stream object
   *
   */
  virtual ~EncodedAudioStream() {
    if (write_buffer != nullptr) {
      delete[] write_buffer;
    }
  }

  /// Define object which need to be notified if the basinfo is changing
  void setNotifyAudioChange(AudioBaseInfoDependent &bi) override {
    TRACEI();
    decoder_ptr->setNotifyAudioChange(bi);
  }

  AudioBaseInfo defaultConfig() {
    AudioBaseInfo cfg;
    cfg.channels = 2;
    cfg.sample_rate = 44100;
    cfg.bits_per_sample = 16;
    return cfg;
  }

  virtual void setAudioInfo(AudioBaseInfo info) override {
    TRACED();
    AudioPrint::setAudioInfo(info);
    decoder_ptr->setAudioInfo(info);
    encoder_ptr->setAudioInfo(info);
  }

  /// Starts the processing - sets the status to active
  void begin(Print *outputStream, AudioEncoder *encoder) {
    TRACED();
    ptr_out = outputStream;
    encoder_ptr = encoder;
    encoder_ptr->setOutputStream(*outputStream);
    writer_ptr = encoder_ptr;
    begin();
  }

  /// Starts the processing - sets the status to active
  void begin(Print *outputStream, AudioDecoder *decoder) {
    TRACED();
    ptr_out = outputStream;
    decoder_ptr = decoder;
    decoder_ptr->setOutputStream(*outputStream);
    writer_ptr = decoder_ptr;
    begin();
  }

  /// Starts the processing - sets the status to active
  void begin() {
    TRACED();
    const CodecNOP *nop = CodecNOP::instance();
    if (decoder_ptr != nop || encoder_ptr != nop) {
      decoder_ptr->begin();
      encoder_ptr->begin();
      active = true;
    } else {
      LOGW("no decoder or encoder defined");
    }
  }

  /// Starts the processing - sets the status to active
  void begin(AudioBaseInfo info) {
    TRACED();
    const CodecNOP *nop = CodecNOP::instance();
    if (decoder_ptr != nop || encoder_ptr != nop) {
      // some decoders need this - e.g. opus
      decoder_ptr->setAudioInfo(info);
      decoder_ptr->begin();
      encoder_ptr->setAudioInfo(info);
      encoder_ptr->begin();
      active = true;
    } else {
      LOGW("no decoder or encoder defined");
    }
  }
  /// Ends the processing
  void end() {
    TRACEI();
    decoder_ptr->end();
    encoder_ptr->end();
    active = false;
  }

  /// encode the data
  virtual size_t write(const uint8_t *data, size_t len) override {
    LOGD("%s: %zu", LOG_METHOD, len);
    if (len == 0) {
      LOGI("write: %d", 0);
      return 0;
    }

    if (writer_ptr == nullptr || data == nullptr) {
      LOGE("NPE");
      return 0;
    }

    size_t result = writer_ptr->write(data, len);
    return result;
  }

  int availableForWrite() override { return ptr_out->availableForWrite(); }

  /// Returns true if status is active and we still have data to be processed
  operator bool() { return active; }

  /// Provides the initialized decoder
  AudioDecoder &decoder() { return *decoder_ptr; }

  /// Provides the initialized encoder
  AudioEncoder &encoder() { return *encoder_ptr; }

protected:
  // ExternalBufferStream ext_buffer;
  AudioDecoder *decoder_ptr = CodecNOP::instance(); // decoder
  AudioEncoder *encoder_ptr = CodecNOP::instance(); // decoder
  AudioWriter *writer_ptr = nullptr;
  Print *ptr_out = nullptr;

  uint8_t *write_buffer = nullptr;
  int write_buffer_pos = 0;
  const int write_buffer_size = 256;
  bool active;
};

} // namespace audio_tools
