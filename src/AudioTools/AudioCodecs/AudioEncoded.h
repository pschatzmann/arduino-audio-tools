#pragma once

#include "AudioCodecsBase.h"
#include "AudioToolsConfig.h"
#include "AudioTools/CoreAudio/AudioLogger.h"
#include "AudioTools/CoreAudio/AudioIO.h"
#include "AudioTools/CoreAudio/AudioOutput.h"
#include "AudioTools/CoreAudio/AudioStreams.h"
#include "AudioTools/CoreAudio/AudioTypes.h"

namespace audio_tools {

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
  EncodedAudioOutput() { active = false; }

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

  virtual ~EncodedAudioOutput() { end(); }

  /// Define object which need to be notified if the basinfo is changing
  void addNotifyAudioChange(AudioInfoSupport &bi) override {
    TRACEI();
    decoder_ptr->addNotifyAudioChange(bi);
  }

  AudioInfo defaultConfig() {
    AudioInfo cfg;
    return cfg;
  }

  virtual void setAudioInfo(AudioInfo newInfo) override {
    TRACED();
    if (this->cfg != newInfo && newInfo) {
      this->cfg = newInfo;
      decoder_ptr->setAudioInfo(cfg);
      encoder_ptr->setAudioInfo(cfg);
    }
  }

  /// Provide audio info from decoder if relevant
  AudioInfo audioInfo() override {
    // return info from decoder if avilable
    if (decoder_ptr != undefined && *decoder_ptr){
      AudioInfo info = decoder_ptr->audioInfo();
      if (info) return info;
    } 
    return ModifyingOutput::audioInfo(); 
  }

  /// Defines the output
  void setOutput(Print *outputStream) {
    ptr_out = outputStream;
    if (decoder_ptr != undefined) {
      decoder_ptr->setOutput(*ptr_out);
    }
    if (encoder_ptr != undefined) {
      encoder_ptr->setOutput(*ptr_out);
    }
  }

  void setOutput(AudioStream* out) { 
    setOutput((Print*)out); 
    to_notify = out;
  }

  void setOutput(AudioOutput*out){
    setOutput((Print*)out);
    to_notify = out;
  }

  void setOutput(Print &outputStream) override { setOutput(&outputStream); }
  void setOutput(AudioOutput &outputStream) { setOutput(&outputStream); }
  void setOutput(AudioStream &outputStream) { setOutput(&outputStream); }

  void setEncoder(AudioEncoder *encoder) {
    if (encoder == nullptr) {
      encoder = undefined;
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
      decoder = undefined;
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
    TRACED();
    if (!active) {
      TRACED();
      // Setup notification
      if (to_notify != nullptr) {
        decoder_ptr->addNotifyAudioChange(*to_notify);
      } 
      // Get notifications from decoder
      decoder_ptr->addNotifyAudioChange(*this);
      if (decoder_ptr != undefined || encoder_ptr != undefined) {
        active = true;
        if (!decoder_ptr->begin(cfg)) active = false;
        if (!encoder_ptr->begin(cfg)) active = false;
      } else {
        LOGW("no decoder or encoder defined");
      }
    }
    return active;
  }

  /// Starts the processing - sets the status to active
  virtual bool begin(AudioInfo newInfo) override {
    cfg = newInfo;
    return begin();
  }

  /// Ends the processing
  void end() override {
    if (active) {
      TRACEI();
      decoder_ptr->end();
      encoder_ptr->end();
      active = false;
    }
  }

  /// encoder decode the data
  virtual size_t write(const uint8_t *data, size_t len) override {
    if (len == 0) {
      // LOGI("write: %d", 0);
      return 0;
    }
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
    return result;
  }

  int availableForWrite() override {
    if (!check_available_for_write) return frame_size;
    return min(ptr_out->availableForWrite(), frame_size);
  }

  /// Returns true if status is active and we still have data to be processed
  operator bool() override { return active; }

  /// Provides the initialized decoder
  AudioDecoder &decoder() { return *decoder_ptr; }

  /// Provides the initialized encoder
  AudioEncoder &encoder() { return *encoder_ptr; }

  /// Is Available for Write check activated ?
  bool isCheckAvailableForWrite() { return check_available_for_write; }

  /// defines the size of the decoded frame in bytes
  void setFrameSize(int size) { frame_size = size; }

  EncodedAudioOutput& operator=(EncodedAudioOutput const& src) {
    decoder_ptr = src.decoder_ptr;
    encoder_ptr = src.encoder_ptr;
    ptr_out = src.ptr_out;
    active = src.active;
    check_available_for_write = src.check_available_for_write;
    frame_size = src.frame_size;
    cfg = src.cfg;
    is_active = src.is_active;
    return *this;
  }


 protected:
  // AudioInfo info;
  CodecNOP* undefined = CodecNOP::instance();
  AudioDecoder *decoder_ptr = undefined;  // decoder
  AudioEncoder *encoder_ptr = undefined;  // decoder
  AudioWriter *writer_ptr = nullptr;
  Print *ptr_out = nullptr;
  AudioInfoSupport *to_notify = nullptr;
  bool active = false;
  bool check_available_for_write = false;
  int frame_size = DEFAULT_BUFFER_SIZE;
};

/// @brief Legacy alias for EncodedAudioOutput
/// @ingroup codecs
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
    setDecoder(decoder);
    setOutput(*outputStream);
  }

  EncodedAudioStream(Print *outputStream, AudioDecoder *decoder) {
    setDecoder(decoder);
    setOutput(*outputStream);
  }

  EncodedAudioStream(Print *outputStream, AudioEncoder *encoder) {
    setEncoder(encoder);
    setOutput(*outputStream);
  }

  EncodedAudioStream(AudioDecoder *decoder) { setDecoder(decoder); }

  EncodedAudioStream(AudioEncoder *encoder) { setEncoder(encoder); }

  virtual ~EncodedAudioStream() { end(); }

  void setEncoder(AudioEncoder *encoder) { enc_out.setEncoder(encoder); }

  void setDecoder(AudioDecoder *decoder) { enc_out.setDecoder(decoder); }

  AudioEncoder *getEncoder() { return enc_out.getEncoder(); }

  AudioDecoder *getDecoder() { return enc_out.getDecoder(); }

  /// Provides the initialized decoder
  AudioDecoder &decoder() { return *getDecoder(); }

  /// Provides the initialized encoder
  AudioEncoder &encoder() { return *getEncoder(); }

  void setStream(Stream *stream) { setStream(*stream); }

  void setStream(AudioStream *stream) { setStream(*stream); }

  void setOutput(AudioOutput *stream) { setOutput(*stream); }

  void setOutput(Print *stream) { setOutput(*stream); }

  void setStream(AudioStream &stream) override {
    ReformatBaseStream::setStream(stream);
    enc_out.setOutput(&stream);
  }

  void setStream(Stream &stream) override {
    ReformatBaseStream::setStream(stream);
    enc_out.setOutput(&stream);
  }

  void setOutput(AudioOutput &stream) override {
    ReformatBaseStream::setOutput(stream);
    enc_out.setOutput(&stream);
  }

  void setOutput(Print &out) override {
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

  bool begin() override {
    // is_output_notify = false;
    setupReader();
    ReformatBaseStream::begin();
    enc_out.addNotifyAudioChange(*this);
    return enc_out.begin(audioInfo());
  }

  void end() override {
    enc_out.end();
    reader.end();
  }

  int availableForWrite() override { return enc_out.availableForWrite(); }

  size_t write(const uint8_t *data, size_t len) override {
    // addNotifyOnFirstWrite();
    return enc_out.write(data, len);
  }

  size_t readBytes(uint8_t *data, size_t len) override {
    return reader.readBytes(data, len);
  }

  void addNotifyAudioChange(AudioInfoSupport &bi) override {
    enc_out.addNotifyAudioChange(bi);
  }

  /// approx compression factor: e.g. mp3 is around 4
  float getByteFactor() override { return byte_factor; }
  void setByteFactor(float factor) { byte_factor = factor; }

  /// defines the size of the decoded frame in bytes
  void setFrameSize(int size) { enc_out.setFrameSize(size); }

  EncodedAudioStream& operator=(EncodedAudioStream const& src) {
    enc_out = src.enc_out;
    byte_factor = src.byte_factor;
    p_stream = src.p_stream;
    p_print = src.p_print;
    info = src.info;
    return *this;
  };

  AudioInfo audioInfo() override { 
    return enc_out.audioInfo();;
  }

  void setAudioInfo(AudioInfo newInfo) override {
    ReformatBaseStream::setAudioInfo(newInfo);
    enc_out.setAudioInfo(newInfo);
  }

 protected:
  EncodedAudioOutput enc_out;
  float byte_factor = 2.0f;
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
  size_t write(const uint8_t *data, size_t len) {
    return p_writer->write(data, len);
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
