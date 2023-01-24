#pragma once

#include "AudioConfig.h"
#include "AudioTools/AudioPrint.h"
#include "AudioTools/AudioStreams.h"
#include "AudioTools/AudioTypes.h"

namespace audio_tools {

/**
 * @brief Docoding of encoded audio into PCM data
 * @ingroup codecs
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
 * @ingroup codecs
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
 * @brief A more natural Print class to process encoded data (aac, wav,
 * mp3...). Just define the output and the decoder and write the encoded 
 * data.
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class EncodedAudioPrint : public AudioStreamX {
public:
  /// Constructor for AudioStream with automatic notification of audio changes
  EncodedAudioPrint(AudioStream *outputStream, AudioDecoder *decoder) {
    TRACED();
    ptr_out = outputStream;
    decoder_ptr = decoder;
    decoder_ptr->setOutputStream(*outputStream);
    decoder_ptr->setNotifyAudioChange(*outputStream);
    writer_ptr = decoder_ptr;
    active = false;
  }

  /// Constructor for AudioPrint with automatic notification of audio changes
  EncodedAudioPrint(AudioPrint *outputStream, AudioDecoder *decoder) {
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
  EncodedAudioPrint(Print &outputStream, AudioDecoder &decoder) {
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
  EncodedAudioPrint(Print *outputStream, AudioDecoder *decoder) {
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
  EncodedAudioPrint(Print &outputStream, AudioEncoder &encoder) {
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
  EncodedAudioPrint(Print *outputStream, AudioEncoder *encoder) {
    TRACED();
    ptr_out = outputStream;
    encoder_ptr = encoder;
    encoder_ptr->setOutputStream(*outputStream);
    writer_ptr = encoder_ptr;
    active = false;
  }


  /**
   * @brief Construct a new Encoded Audio Stream object - the Output and
   * Encoder/Decoder needs to be defined with the begin method.
   */
  EncodedAudioPrint() {
    TRACED();
    active = false;
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
    AudioStream::setAudioInfo(info);
    decoder_ptr->setAudioInfo(info);
    encoder_ptr->setAudioInfo(info);
  }

  /// Starts the processing - sets the status to active
  bool begin(Print *outputStream, AudioEncoder *encoder) {
    TRACED();
    ptr_out = outputStream;
    encoder_ptr = encoder;
    encoder_ptr->setOutputStream(*outputStream);
    writer_ptr = encoder_ptr;
    return begin();
  }

  /// Starts the processing - sets the status to active
  bool begin(Print *outputStream, AudioDecoder *decoder) {
    TRACED();
    ptr_out = outputStream;
    decoder_ptr = decoder;
    decoder_ptr->setOutputStream(*outputStream);
    writer_ptr = decoder_ptr;
    return begin();
  }

  /// Starts the processing - sets the status to active
  bool begin() override {
    TRACED();
    const CodecNOP *nop = CodecNOP::instance();
    if (decoder_ptr != nop || encoder_ptr != nop) {
      decoder_ptr->begin();
      encoder_ptr->begin();
      active = true;
    } else {
      LOGW("no decoder or encoder defined");
    }
    return active;
  }

  /// Starts the processing - sets the status to active
  bool begin(AudioBaseInfo cfg) {
    TRACED();
    info = cfg;
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
  AudioDecoder *decoder_ptr = CodecNOP::instance(); // decoder
  AudioEncoder *encoder_ptr = CodecNOP::instance(); // decoder
  AudioWriter *writer_ptr = nullptr;
  Print *ptr_out = nullptr;
  bool active;
};

/**
 * @brief A more natural Stream class to process encoded data (aac, wav,
 * mp3...) which also supports the decoding by calling readBytes().
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class EncodedAudioStream : public EncodedAudioPrint {
public:
  EncodedAudioStream(AudioStream *ioStream, AudioDecoder *decoder) : EncodedAudioPrint(ioStream, decoder) { 
    // the indicated stream can be used as input
    setInput(ioStream);
  }

  EncodedAudioStream(Stream *ioStream, AudioDecoder *decoder) : EncodedAudioPrint((Print*)ioStream, decoder) { 
    // the indicated stream can be used as input
    setInput(ioStream);
  }

  EncodedAudioStream(AudioDecoder *decoder) : EncodedAudioPrint() { 
    decoder_ptr = decoder;
    writer_ptr = decoder_ptr;
    active = false;
  }

  /// Constructor for AudioPrint with automatic notification of audio changes
  EncodedAudioStream(AudioPrint *outputStream, AudioDecoder *decoder) : EncodedAudioPrint(outputStream, decoder) {  }

  /**
   * @brief Construct a new Encoded Stream object - used for decoding
   *
   * @param outputStream
   * @param decoder
   */
  EncodedAudioStream(Print &outputStream, AudioDecoder &decoder) : EncodedAudioPrint(outputStream, decoder) {  }

  /**
   * @brief Construct a new Encoded Audio Stream object - used for decoding
   *
   * @param outputStream
   * @param decoder
   */
  EncodedAudioStream(Print *outputStream, AudioDecoder *decoder) : EncodedAudioPrint(outputStream, decoder) {  }

  /**
   * @brief Construct a new Encoded Audio Stream object - used for encoding
   *
   * @param outputStream
   * @param encoder
   */
  EncodedAudioStream(Print &outputStream, AudioEncoder &encoder) : EncodedAudioPrint(outputStream, encoder) { }

  /**
   * @brief Construct a new Encoded Audio Stream object - used for encoding
   *
   * @param outputStream
   * @param encoder
   */
  EncodedAudioStream(Print *outputStream, AudioEncoder *encoder) : EncodedAudioPrint(outputStream, encoder) {}

  /**
   * @brief Construct a new Encoded Audio Stream object - the Output and
   * Encoder/Decoder needs to be defined with the begin method
   *
   */
  EncodedAudioStream() : EncodedAudioPrint() {}

  /**
   * @brief Defines the input stream for decoding
   * 
   * @param ioStream 
   */
  void setInput(Stream *ioStream){
    TRACED();
    p_stream = ioStream;
  }

  /// @brief Defines the buffer size
  void resize(int size){
    decoded_buffer.resize(size);
  }

  /// @brief setup default size for buffer
  void resize(){
    resize(1024*10);
  }

  int available() override {
    if (p_stream==nullptr) return 0;
    decode(reqested_bytes);
    return decoded_buffer.available();
  }

  size_t readBytes(uint8_t *buffer, size_t length) override {
    TRACED();
    if (p_stream==nullptr) return 0;
    decode(reqested_bytes);
    return decoded_buffer.readArray(buffer, length);
  }

protected:
  RingBuffer<uint8_t> decoded_buffer{0}; 
  QueueStream<uint8_t> out_stream{decoded_buffer};
  Vector<uint8_t> copy_buffer{DEFAULT_BUFFER_SIZE};
  Stream *p_stream = nullptr;
  int reqested_bytes = DEFAULT_BUFFER_SIZE;
  bool is_setup = false;
  int max_read_count = 5;
 
  // Fill the decoded_buffer so that we have data for readBytes call
  void decode(int requestedBytes){
    TRACED();
    // setup buffer once
    setupOnce();
    // fill decoded_buffer if we do not have enough data
    if(p_stream->available()>0 && decoded_buffer.available()<reqested_bytes){
      for (int j=0;j<max_read_count;j++){
        int bytes_read = p_stream->readBytes(copy_buffer.data(), DEFAULT_BUFFER_SIZE);
        int result = decoder_ptr->write(copy_buffer.data(), bytes_read);
        if (p_stream->available()==0 || decoded_buffer.available()>=reqested_bytes){
          break;
        }
      }
    } 
    LOGD("available decoded data %d", decoded_buffer.available());
  }

  void setupOnce(){
    if (!is_setup){
      is_setup = true;
      LOGI("Setup reading support");
      resize();
      // make sure the result goes to out_stream
      decoder_ptr->setOutputStream(out_stream);
      writer_ptr = decoder_ptr;
      out_stream.begin();
    }
  }
};


} // namespace audio_tools