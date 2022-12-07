#pragma once

#include "AudioCodecs/CodecOpus.h"
#include "AudioCodecs/AudioEncoded.h"
#include "AudioTools/Buffers.h"
#include "oggz/oggz.h"

#define OGG_DEFAULT_BUFFER_SIZE (246)
#define OGG_READ_SIZE (512)

namespace audio_tools {

/**
 * @brief OggContainerDecoder - Ogg Container. Decodes a packet from an Ogg container.
 * The Ogg begin segment contains the AudioBaseInfo structure. You can subclass
 * and overwrite the beginOfSegment() method to implement your own headers
 * @ingroup codecs
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class OggContainerDecoder : public AudioDecoder {
 public:
  /**
   * @brief Construct a new OggContainerDecoder object
   */

  OggContainerDecoder() { TRACED(); }

  OggContainerDecoder(AudioDecoder *decoder) {
    p_codec = decoder;
  }

  OggContainerDecoder(AudioDecoder &decoder) {
    p_codec = &decoder;
  }
  ~OggContainerDecoder(){
    if (p_codec!=nullptr) delete p_print;
  }

  /// Defines the output Stream
  void setOutputStream(Print &out_stream) override { 
    if (p_codec==nullptr) {
      p_print = &out_stream;
    } else {
      EncodedAudioStream* eas = new EncodedAudioStream();
      eas->begin(&out_stream, p_codec);
      p_print = eas;
    }
  }

  void setNotifyAudioChange(AudioBaseInfoDependent &bi) override {
    this->bid = &bi;
  }

  AudioBaseInfo audioInfo() override { return cfg; }

  void begin(AudioBaseInfo info) {
    TRACED();
    cfg = info;
    if (bid != nullptr) {
      bid->setAudioInfo(cfg);
    }
    begin();
  }

  void begin() override {
    TRACED();
    if (p_oggz == nullptr) {
      p_oggz = oggz_new(OGGZ_READ |  OGGZ_AUTO); // OGGZ_NONSTRICT
      is_open = true;
      // Callback to Replace standard IO
      if (oggz_io_set_read(p_oggz, ogg_io_read, this)!=0){
        LOGE("oggz_io_set_read");
        is_open = false;
      }
      // Callback
      if (oggz_set_read_callback(p_oggz, -1, read_packet, this)!=0){
        LOGE("oggz_set_read_callback");
        is_open = false;
      }

      if (oggz_set_read_page(p_oggz, -1, read_page, this)!=0){
        LOGE("oggz_set_read_page");
        is_open = false;
      }
    }
  }

  void end() override {
    TRACED();
    flush();
    if (p_codec!=nullptr) p_codec->end();
    is_open = false;
    oggz_close(p_oggz);
    p_oggz = nullptr;
  }

  void flush() {
      LOGD("oggz_read...");
      while ((oggz_read(p_oggz, OGG_READ_SIZE)) > 0)
        ;
  }

  virtual size_t write(const void *in_ptr, size_t in_size) override {
    LOGD("write: %u", in_size);
    if (p_print == nullptr) return 0;

    // fill buffer
    size_t size_consumed = buffer.writeArray((uint8_t *)in_ptr, in_size);
    if (buffer.availableForWrite()==0){
      // Read all bytes into oggz, calling any read callbacks on the fly.
      flush();
    }
    // write remaining bytes
    if (size_consumed<in_size){
      size_consumed += buffer.writeArray((uint8_t *)in_ptr+size_consumed, in_size-size_consumed);
    }
    return size_consumed;
  }

  virtual operator bool() override { return is_open; }

 protected:
  AudioDecoder *p_codec = nullptr;
  RingBuffer<uint8_t> buffer{OGG_DEFAULT_BUFFER_SIZE};
  Print *p_print = nullptr;
  OGGZ *p_oggz = nullptr;
  AudioBaseInfoDependent *bid = nullptr;
  AudioBaseInfo cfg;
  bool is_open = false;
  long pos = 0;

  // Final Stream Callback -> write data to requested output destination
  static size_t ogg_io_read(void *user_handle, void *buf, size_t n) {
    LOGI("ogg_io_read: %u", n);
    OggContainerDecoder *self = (OggContainerDecoder *)user_handle;
    int len = self->buffer.readArray((uint8_t *)buf, n);
    self->pos += len;
    return len;
    //return self->p_print->write((uint8_t *)buf, n);
  }
  
  // Process full packet
  static int read_packet(OGGZ *oggz, oggz_packet *zp, long serialno,
                         void *user_data) {
    LOGI("read_packet: %u", zp->op.bytes);
    OggContainerDecoder *self = (OggContainerDecoder *)user_data;
    ogg_packet *op = &zp->op;
    int result = op->bytes;
    if (op->b_o_s) {
      self->beginOfSegment(op);
    } else if (op->e_o_s) {
      self->endOfSegment(op);
    } else {
      LOGI("process audio packet");
      int eff = self->p_print->write(op->packet, op->bytes);
      if (eff!=result){
        LOGE("Incomplere write");
      }
    }
    return result;
  }


 static int read_page(OGGZ *oggz, const ogg_page *og, long serialno, void *user_data){
    LOGI("read_page: %u", og->body_len);
    return og->body_len;
 }

  virtual void beginOfSegment(ogg_packet *op) {
    LOGD("bos");
    if (op->bytes >= sizeof(cfg)) {
      memcpy(&cfg, op->packet, sizeof(cfg));
      cfg.logInfo();
      notify();
    }
  }

  virtual void endOfSegment(ogg_packet *op) {
    // end segment not supported
    LOGW("e_o_s");
  }

  virtual void notify() {
    if (bid != nullptr) {
      bid->setAudioInfo(cfg);
    }
  }
};

/**
 * @brief OggContainerEncoder - Ogg Container. Encodes a packet for an Ogg container.
 * The Ogg begin segment contains the AudioBaseInfo structure. You can subclass
 * ond overwrite the writeHeader() method to implement your own header logic.
 * @ingroup codecs
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class OggContainerEncoder : public AudioEncoder {
 public:
  // Empty Constructor - the output stream must be provided with begin()
  OggContainerEncoder() {}

  OggContainerEncoder(AudioEncoder *encoder) {
    p_codec = encoder;
  }

  OggContainerEncoder(AudioEncoder &encoder) {
    p_codec = &encoder;
  }

  ~OggContainerEncoder(){
    if (p_codec!=nullptr) delete p_print;
  }

  /// Defines the output Stream
  void setOutputStream(Print &out_stream) override { 
    if (p_codec==nullptr) {
      p_print = &out_stream;
    } else {
      EncodedAudioStream* eas = new EncodedAudioStream();
      eas->begin(&codec_buffer, p_codec);
      p_encoded_audio_stream = eas;
      p_print = &out_stream;
    }
  }

  /// Provides "audio/pcm"
  const char *mime() override { return mime_pcm; }

  /// We actually do nothing with this
  virtual void setAudioInfo(AudioBaseInfo from) override { cfg = from; }

  virtual void begin(AudioBaseInfo from) {
    setAudioInfo(cfg);
    begin();
  }

  /// starts the processing using the actual AudioInfo
  virtual void begin() override {
    TRACED();
    is_open = true;
    codec_buffer.begin();
    if (p_oggz == nullptr) {
      p_oggz = oggz_new(OGGZ_WRITE | OGGZ_NONSTRICT | OGGZ_AUTO);
      serialno = oggz_serialno_new(p_oggz);
      oggz_io_set_write(p_oggz, ogg_io_write, this);
      packetno = 0;
      granulepos = 0;

      if (!writeHeader()){
        is_open = false;
      }
    }
  }

  /// starts the processing
  void begin(Print &out) {
    p_print = &out;
    begin();
  }

  /// stops the processing
  void end() override {
    TRACED();

    writeFooter();

    is_open = false;
    oggz_close(p_oggz);
    p_oggz = nullptr;
  }

  /// Writes raw data to be encoded and packaged
  virtual size_t write(const void *in_ptr, size_t in_size) override {
    if (!is_open || p_print == nullptr) return 0;
    LOGD("write: %d", (int) in_size);

    if (p_codec!=nullptr){
      // encode the data
      size_t eff = p_encoded_audio_stream->write((uint8_t*)in_ptr, in_size);
      if (eff!=in_size){
        LOGE("Write overflow");
      }
      // get the result from the buffer
      void *encoded_data = buffer.address();
      int enoded_size = buffer.available();

      op.packet = (uint8_t *)encoded_data;
      op.bytes = enoded_size;
    } else {
      op.packet = (uint8_t *)in_ptr;
      op.bytes = in_size;
    }
    if (op.bytes>0){
      buffer.reset();
      op.granulepos = granulepos +=
          in_size / sizeof(int16_t) / cfg.channels;  // sample
      op.b_o_s = false;
      op.e_o_s = false;
      op.packetno = packetno++;
      is_audio = true;
      if (!writePacket(op, OGGZ_FLUSH_AFTER)) {
        return 0;
      }
    }
    // trigger pysical write
    while ((oggz_write(p_oggz, in_size)) > 0)
      ;

    return in_size;
  }

  operator bool() override { return is_open; }

  bool isOpen() { return is_open; }

 protected:
  Print *p_print = nullptr;
  Print *p_encoded_audio_stream = nullptr;
  SingleBuffer<uint8_t> buffer{1024};
  QueueStream<uint8_t> codec_buffer{buffer};
  AudioEncoder* p_codec = nullptr;
  volatile bool is_open;
  OGGZ *p_oggz = nullptr;
  ogg_packet op;
  ogg_packet oh;
  size_t granulepos = 0;
  size_t packetno = 0;
  long serialno = -1;
  AudioBaseInfo cfg;
  bool is_audio = false;

  virtual bool writePacket(ogg_packet &op, int flag = 0) {
    LOGD("writePacket: %d", (int) op.bytes);
    long result = oggz_write_feed(p_oggz, &op, serialno, flag, NULL);
    if (result < 0) {
      LOGE("oggz_write_feed: %d", (int) result);
      return false;
    }
    return true;
  }

  virtual bool writeHeader() {
    TRACED();
    oh.packet = (uint8_t *)&cfg;
    oh.bytes = sizeof(cfg);
    oh.granulepos = 0;
    oh.packetno = packetno++;
    oh.b_o_s = true;
    oh.e_o_s = false;
    is_audio = false;
    return writePacket(oh);
  }

  virtual bool writeFooter() {
    TRACED();
    op.packet = (uint8_t *)nullptr;
    op.bytes = 0;
    op.granulepos = granulepos;
    op.packetno = packetno++;
    op.b_o_s = false;
    op.e_o_s = true;
    is_audio = false;
    return writePacket(op, OGGZ_FLUSH_AFTER);
  }

  // Final Stream Callback
  static size_t ogg_io_write(void *user_handle, void *buf, size_t n) {
    LOGD("ogg_io_write: %d", (int) n);
    OggContainerEncoder *self = (OggContainerEncoder *)user_handle;
    if (self == nullptr) {
      LOGE("self is null");
      return 0;
    }
    if (self->p_print == nullptr) {
      LOGE("p_print is null");
      return 0;
    }
    return self->p_print->write((uint8_t *)buf, n);
  }
};

}  // namespace audio_tools
