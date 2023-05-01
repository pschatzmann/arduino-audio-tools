#pragma once

#include "AudioConfig.h"
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

  // for most decoder this is not needed
  virtual void setAudioInfo(AudioInfo from) override {
    TRACED();
    if (info != from) {
      if (p_notify != nullptr) {
        p_notify->setAudioInfo(from);
      }
    }
    info = from;
  }
  virtual void setOutputStream(AudioStream &out_stream) {
    Print *p_print = &out_stream;
    setOutputStream(*p_print);
    setNotifyAudioChange(out_stream);
  }

  virtual void setOutputStream(AudioOutput &out_stream) {
    Print *p_print = &out_stream;
    setOutputStream(*p_print);
    setNotifyAudioChange(out_stream);
  }

  virtual void setOutputStream(Print &out_stream) override {
    p_print = &out_stream;
  }
  // Th decoding result is PCM data
  virtual bool isResultPCM() { return true; }

  /// Registers an object that is notified if the audio format is changing
  void setNotifyAudioChange(AudioInfoSupport &notify) override {
    p_notify = &notify;
  }

 protected:
  Print *p_print = nullptr;
  AudioInfo info;
  AudioInfoSupport *p_notify = nullptr;
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
  virtual const char *mime() = 0;
  void setAudioInfo(AudioInfo from) override{};
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

  virtual void begin() {}
  virtual void end() {}
  virtual void setOutputStream(Print &out_stream) {}
  virtual void setNotifyAudioChange(AudioInfoSupport &bi) {}
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
class StreamingDecoder {
 public:
  /// Starts the processing
  virtual void begin() = 0;

  /// Releases the reserved memory
  virtual void end() = 0;

  /// Defines the output Stream
  virtual void setOutputStream(Print &outStream) = 0;

  /// Register Output Stream to be notified about changes
  virtual void setNotifyAudioChange(AudioInfoSupport &bi) = 0;

  /// Defines the output streams and register to be notified
  virtual void setOutputStream(AudioStream &out_stream) {
    Print *p_print = &out_stream;
    setOutputStream(*p_print);
    setNotifyAudioChange(out_stream);
  }

  /// Defines the output streams and register to be notified
  virtual void setOutputStream(AudioOutput &out_stream) {
    Print *p_print = &out_stream;
    setOutputStream(*p_print);
    setNotifyAudioChange(out_stream);
  }

  /// Defines the input data stream
  virtual void setInputStream(Stream &inStream) = 0;

  /// Provides the last available MP3FrameInfo
  virtual AudioInfo audioInfo() = 0;

  /// checks if the class is active
  virtual operator bool() = 0;

  /// Process a single read operation - to be called in the loop
  virtual bool copy() = 0;

 protected:
  virtual size_t readBytes(uint8_t *buffer, size_t len) = 0;
};

/**
 * @brief A more natural Print class to process encoded data (aac, wav,
 * mp3...). Just define the output and the decoder and write the encoded
 * data.
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class EncodedAudioOutput : public AudioStream {
 public:
  /// Constructor for AudioStream with automatic notification of audio changes
  EncodedAudioOutput(AudioStream *outputStream, AudioDecoder *decoder) {
    TRACED();
    ptr_out = outputStream;
    decoder_ptr = decoder;
    decoder_ptr->setOutputStream(*outputStream);
    decoder_ptr->setNotifyAudioChange(*outputStream);
    writer_ptr = decoder_ptr;
    active = false;
  }

  /// Constructor for AudioOutput with automatic notification of audio changes
  EncodedAudioOutput(AudioOutput *outputStream, AudioDecoder *decoder) {
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
  EncodedAudioOutput(Print &outputStream, AudioDecoder &decoder) {
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
  EncodedAudioOutput(Print *outputStream, AudioDecoder *decoder) {
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
  EncodedAudioOutput(Print &outputStream, AudioEncoder &encoder) {
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
  EncodedAudioOutput(Print *outputStream, AudioEncoder *encoder) {
    TRACED();
    ptr_out = outputStream;
    encoder_ptr = encoder;
    encoder_ptr->setOutputStream(*outputStream);
    writer_ptr = encoder_ptr;
    active = false;
  }

  EncodedAudioOutput(AudioOutput *outputStream, AudioEncoder *encoder) {
    TRACED();
    ptr_out = outputStream;
    encoder_ptr = encoder;
    encoder_ptr->setOutputStream(*outputStream);
    writer_ptr = encoder_ptr;
    active = false;
  }

  EncodedAudioOutput(AudioStream *outputStream, AudioEncoder *encoder) {
    TRACED();
    ptr_out = outputStream;
    encoder_ptr = encoder;
    encoder_ptr->setOutputStream(*outputStream);
    writer_ptr = encoder_ptr;
    active = false;
  }

  /**
   * @brief Construct a new Encoded Audio Stream object - the Output and
   * Encoder/Decoder needs to be defined with the corresponding setter methods.
   */
  EncodedAudioOutput() {
    TRACED();
    active = false;
  }

  /// Define object which need to be notified if the basinfo is changing
  void setNotifyAudioChange(AudioInfoSupport &bi) override {
    TRACEI();
    decoder_ptr->setNotifyAudioChange(bi);
  }

  AudioInfo defaultConfig() {
    AudioInfo cfg;
    cfg.channels = 2;
    cfg.sample_rate = 44100;
    cfg.bits_per_sample = 16;
    return cfg;
  }

  virtual void setAudioInfo(AudioInfo info) override {
    TRACED();
    if (this->info != info) {
      this->info = info;
      AudioStream::setAudioInfo(info);
      decoder_ptr->setAudioInfo(info);
      encoder_ptr->setAudioInfo(info);
    }
  }

  /// Defines the output
  void setOutput(Print *outputStream) { ptr_out = outputStream; }

  /// The same as setOutput
  void setStream(Print *outputStream) { setOutput(outputStream); }

  void setEncoder(AudioEncoder *encoder) {
    if (encoder == nullptr) {
      encoder = CodecNOP::instance();
    }
    encoder_ptr = encoder;
    writer_ptr = encoder;
    if (ptr_out != nullptr) {
      encoder_ptr->setOutputStream(*ptr_out);
    }
  }

  void setDecoder(AudioDecoder *decoder) {
    if (decoder == nullptr) {
      decoder = CodecNOP::instance();
    }
    decoder_ptr = decoder;
    writer_ptr = decoder;
    if (ptr_out != nullptr) {
      decoder_ptr->setOutputStream(*ptr_out);
    }
  }

  /// Starts the processing - sets the status to active
  bool begin() override {
    TRACED();

    if (!active) {
      const CodecNOP *nop = CodecNOP::instance();
      if (decoder_ptr != nop || encoder_ptr != nop) {
        active = true;
        decoder_ptr->begin();
        encoder_ptr->begin();
      } else {
        LOGW("no decoder or encoder defined");
      }
    }
    return active;
  }

  /// Starts the processing - sets the status to active
  bool begin(AudioInfo cfg) {
    TRACED();
    info = cfg;
    const CodecNOP *nop = CodecNOP::instance();
    if (decoder_ptr != nop || encoder_ptr != nop) {
      // some decoders need this - e.g. opus
      decoder_ptr->begin(info);
      encoder_ptr->begin(info);
      active = true;
    } else {
      LOGW("no decoder or encoder defined");
    }
    return active;
  }

  /// Ends the processing
  void end() override {
    TRACEI();
    decoder_ptr->end();
    encoder_ptr->end();
    active = false;
  }

  /// encodeor decode the data
  virtual size_t write(const uint8_t *data, size_t len) override {
    LOGD("EncodedAudioOutput::write: %zu", len);
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
  AudioInfo info;
  AudioDecoder *decoder_ptr = CodecNOP::instance();  // decoder
  AudioEncoder *encoder_ptr = CodecNOP::instance();  // decoder
  AudioWriter *writer_ptr = nullptr;
  Print *ptr_out = nullptr;
  bool active;
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
class EncodedAudioStream : public EncodedAudioOutput {
 public:
  EncodedAudioStream(AudioStream *ioStream, AudioDecoder *decoder)
      : EncodedAudioOutput(ioStream, decoder) {
    // the indicated stream can be used as input
    setStream(ioStream);
  }

  EncodedAudioStream(Stream *ioStream, AudioDecoder *decoder)
      : EncodedAudioOutput((Print *)ioStream, decoder) {
    // the indicated stream can be used as input
    setStream(ioStream);
  }

  EncodedAudioStream(AudioDecoder *decoder) : EncodedAudioOutput() {
    decoder_ptr = decoder;
    writer_ptr = decoder_ptr;
    active = false;
  }

  /// Constructor for AudioOutput with automatic notification of audio changes
  EncodedAudioStream(AudioOutput *outputStream, AudioDecoder *decoder)
      : EncodedAudioOutput(outputStream, decoder) {}

  /**
   * @brief Construct a new Encoded Stream object - used for decoding
   *
   * @param outputStream
   * @param decoder
   */
  EncodedAudioStream(Print &outputStream, AudioDecoder &decoder)
      : EncodedAudioOutput(outputStream, decoder) {}

  /**
   * @brief Construct a new Encoded Audio Stream object - used for decoding
   *
   * @param outputStream
   * @param decoder
   */
  EncodedAudioStream(Print *outputStream, AudioDecoder *decoder)
      : EncodedAudioOutput(outputStream, decoder) {}

  /**
   * @brief Construct a new Encoded Audio Stream object - used for encoding
   *
   * @param outputStream
   * @param encoder
   */
  EncodedAudioStream(Print &outputStream, AudioEncoder &encoder)
      : EncodedAudioOutput(outputStream, encoder) {}

  /**
   * @brief Construct a new Encoded Audio Stream object - used for encoding
   */
  EncodedAudioStream(Stream &io, AudioEncoder &encoder)
      : EncodedAudioOutput(io, encoder) {
    setStream(&io);
  }

  /**
   * @brief Construct a new Encoded Audio Stream object - used for encoding
   */
  EncodedAudioStream(AudioStream &io, AudioEncoder &encoder)
      : EncodedAudioOutput(io, encoder) {
    setStream(&io);
  }

  /**
   * @brief Construct a new Encoded Audio Stream object - used for encoding
   *
   * @param outputStream
   * @param encoder
   */
  EncodedAudioStream(Print *outputStream, AudioEncoder *encoder)
      : EncodedAudioOutput(outputStream, encoder) {}

  /**
   * @brief Construct a new Encoded Audio Stream object - the Output and
   * Encoder/Decoder needs to be defined with the corresponding setter methods
   *
   */
  EncodedAudioStream() : EncodedAudioOutput() {}

  /// Same as setStream()
  void setInput(Stream *ioStream) { setStream(ioStream); }

  /// Defines the input/output stream for decoding
  void setStream(Stream *ioStream) {
    TRACED();
    EncodedAudioOutput::setStream(ioStream);
    p_stream = ioStream;
  }

  void setEncoder(AudioEncoder *encoder) {
    EncodedAudioOutput::setEncoder(encoder);
    is_setup = false;
  }

  void setDecoder(AudioDecoder *decoder) {
    EncodedAudioOutput::setDecoder(decoder);
    is_setup = false;
  }

  /// @brief Defines the buffer size
  void resize(int size) { decoded_buffer.resize(size); }

  /// @brief setup default size for buffer
  void resize() { resize(1024 * 10); }

  int available() override {
    if (p_stream == nullptr) return 0;
    decode(reqested_bytes);
    return decoded_buffer.available();
  }

  size_t readBytes(uint8_t *buffer, size_t length) override {
    LOGD("EncodedAudioStream::readBytes: %d", (int)length);
    if (p_stream == nullptr) {
      TRACEE();
      return 0;
    }
    decode(reqested_bytes);
    return decoded_buffer.readArray(buffer, length);
  }

 protected:
  RingBuffer<uint8_t> decoded_buffer{0};
  QueueStream<uint8_t> queue_stream{decoded_buffer};
  Vector<uint8_t> copy_buffer{DEFAULT_BUFFER_SIZE};
  Stream *p_stream = nullptr;
  AudioWriter *p_write = nullptr;
  int reqested_bytes = DEFAULT_BUFFER_SIZE;
  bool is_setup = false;
  int max_read_count = 5;

  // Fill the decoded_buffer so that we have data for readBytes call
  void decode(int requestedBytes) {
    TRACED();
    // setup buffer once
    setupOnce();

    // fill decoded_buffer if we do not have enough data
    if (p_stream->available() > 0 &&
        decoded_buffer.available() < reqested_bytes) {
      for (int j = 0; j < max_read_count; j++) {
        int bytes_read =
            p_stream->readBytes(copy_buffer.data(), DEFAULT_BUFFER_SIZE);
        LOGD("bytes_read: %d", bytes_read);
        int result = writer_ptr->write(copy_buffer.data(), bytes_read);
        if (p_stream->available() == 0 ||
            decoded_buffer.available() >= reqested_bytes) {
          break;
        }
      }
    }
    LOGD("available decoded data %d", decoded_buffer.available());
  }

  void setupOnce() {
    if (!is_setup) {
      is_setup = true;
      LOGI("Setup reading support");
      resize();
      // make sure the result goes to out_stream
      writer_ptr->setOutputStream(queue_stream);
      queue_stream.begin();
    }
  }
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
    if (this->info != info) {
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
        p_writer1->setOutputStream(print2);
        p_writer2->setOutputStream(*p_print);
        p_writer1->begin();
        p_writer2->begin();
      } else {
        p_writer1->setOutputStream(*p_print);
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
