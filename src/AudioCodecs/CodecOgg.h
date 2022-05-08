#pragma once

#include "AudioCodecs/CodecOpus.h"
#include "AudioTools/AudioTypes.h"
#include "AudioTools/Buffers.h"
#include "oggz/oggz.h"

#define OGG_DEFAULT_BUFFER_SIZE (2048)

namespace audio_tools {

/**
 * @brief OggDecoder - Ogg Container. Decodes a packet from an Ogg container.
 * The Ogg begin segment contains the AudioBaseInfo structure. You can subclass
 * and overwrite the beginOfSegment() method to implement your own headers
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class OggDecoder : public AudioDecoder {
 public:
  /**
   * @brief Construct a new OggDecoder object
   */

  OggDecoder() { LOGD(LOG_METHOD); }

  /// Defines the output Stream
  void setOutputStream(Print &out_stream) override { p_print = &out_stream; }

  void setNotifyAudioChange(AudioBaseInfoDependent &bi) override {
    this->bid = &bi;
  }

  AudioBaseInfo audioInfo() override { return cfg; }

  void begin(AudioBaseInfo info) {
    LOGD(LOG_METHOD);
    cfg = info;
    if (bid != nullptr) {
      bid->setAudioInfo(cfg);
    }
    is_open = true;
  }

  void begin() override {
    LOGD(LOG_METHOD);
    if (p_oggz == nullptr) {
      p_oggz = oggz_new(OGGZ_READ | OGGZ_AUTO);
      // Callback to Replace standard IO
      oggz_io_set_read(p_oggz, ogg_io_read, this);
      oggz_io_set_seek(p_oggz, ogg_io_seek, this);
      oggz_io_set_tell(p_oggz, ogg_io_tell, this);

      // Callback
      oggz_set_read_callback(p_oggz, -1, read_packet, this);
    }

    is_open = true;
  }

  void end() override {
    LOGD(LOG_METHOD);
    is_open = false;
    oggz_close(p_oggz);
    p_oggz = nullptr;
  }

  virtual size_t write(const void *in_ptr, size_t in_size) override {
    LOGI("write: %u", in_size);
    if (p_print == nullptr) return 0;
    size_t result = buffer.writeArray((uint8_t *)in_ptr, in_size);

    // Read all bytes into oggz, calling any read callbacks on the fly.
    LOGD("oggz_read...");
    while ((oggz_read(p_oggz, in_size)) > 0)
      ;

    return result;
  }

  virtual operator boolean() override { return is_open; }

 protected:
  RingBuffer<uint8_t> buffer{OGG_DEFAULT_BUFFER_SIZE};
  Print *p_print = nullptr;
  OGGZ *p_oggz = nullptr;
  AudioBaseInfoDependent *bid = nullptr;
  AudioBaseInfo cfg;
  bool is_open = false;
  long pos = 0;

  // Final Stream Callback
  static size_t ogg_io_read(void *user_handle, void *buf, size_t n) {
    LOGI("ogg_io_read: %u", n);
    OggDecoder *self = (OggDecoder *)user_handle;
    int len = self->buffer.readArray((uint8_t *)buf, n);
    self->pos += len;
    return len;
  }
  static int ogg_io_seek(void *user_handle, long offset, int whence) {
    LOGI("ogg_io_seek: %u", offset);
    return -1;
  }

  static long ogg_io_tell(void *user_handle) {
    LOGI(LOG_METHOD);
    OggDecoder *self = (OggDecoder *)user_handle;
    return self->pos;
  }

  // Process full packet
  static int read_packet(OGGZ *oggz, oggz_packet *zp, long serialno,
                         void *user_data) {
    OggDecoder *self = (OggDecoder *)user_data;
    ogg_packet *op = &zp->op;
    int result = op->bytes;
    LOGI("read_packet: %u", result);
    if (op->b_o_s) {
      self->beginOfSegment(op);
    } else if (op->e_o_s) {
      self->endOfSegment(op);
    } else {
      LOGD("audio");
      self->p_print->write(op->packet, op->bytes);
    }
    return result;
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
 * @brief OggEncoder - Ogg Container. Encodes a packet for an Ogg container.
 * The Ogg begin segment contains the AudioBaseInfo structure. You can subclass
 * ond overwrite the writeHeader() method to implement your own header logic.
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class OggEncoder : public AudioEncoder {
 public:
  // Empty Constructor - the output stream must be provided with begin()
  OggEncoder() {}

  /// Defines the output Stream
  void setOutputStream(Print &out_stream) override { p_print = &out_stream; }

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
    LOGD(LOG_METHOD);
    is_open = true;
    if (p_oggz == nullptr) {
      p_oggz = oggz_new(OGGZ_WRITE);
      serialno = oggz_serialno_new(p_oggz);
      oggz_io_set_write(p_oggz, ogg_io_write, this);
      packetno = 0;
      granulepos = 0;

      writeHeader();
    }
  }

  /// starts the processing
  void begin(Print &out) {
    p_print = &out;
    begin();
  }

  /// stops the processing
  void end() override {
    LOGD(LOG_METHOD);

    writeFooter();

    is_open = false;
    oggz_close(p_oggz);
    p_oggz = nullptr;
  }

  /// Writes Ogg Packet
  virtual size_t write(const void *in_ptr, size_t in_size) override {
    if (!is_open || p_print == nullptr) return 0;
    LOGD("write: %u", in_size);

    op.packet = (uint8_t *)in_ptr;
    op.bytes = in_size;
    op.granulepos = granulepos +=
        in_size / sizeof(int16_t) / cfg.channels;  // sample
    op.b_o_s = false;
    op.e_o_s = false;
    op.packetno = packetno++;
    if (!writePacket(in_size)) {
      return 0;
    }

    // trigger pysical write
    while ((oggz_write(p_oggz, in_size)) > 0)
      ;

    return in_size;
  }

  operator boolean() override { return is_open; }

  bool isOpen() { return is_open; }

 protected:
  Print *p_print = nullptr;
  volatile bool is_open;
  OGGZ *p_oggz = nullptr;
  ogg_packet op;
  size_t granulepos = 0;
  size_t packetno = 0;
  long serialno = -1;
  AudioBaseInfo cfg;

  virtual bool writePacket(int in_size, int flag = 0) {
    LOGD("oggz_write_feed: %u", in_size);
    long result = oggz_write_feed(p_oggz, &op, serialno, flag, NULL);
    if (result < 0) {
      LOGE("oggz_write_feed: %d", result);
      return false;
    }
    return true;
  }

  virtual void writeHeader() {
    op.packet = (uint8_t *)&cfg;
    op.bytes = sizeof(cfg);
    op.granulepos = 0;
    op.packetno = packetno++;
    op.b_o_s = true;
    op.e_o_s = false;
    writePacket(op.bytes);
  }

  virtual void writeFooter() {
    op.packet = (uint8_t *)nullptr;
    op.bytes = 0;
    op.granulepos = granulepos;
    op.packetno = packetno++;
    op.b_o_s = false;
    op.e_o_s = true;
    writePacket(0, OGGZ_FLUSH_AFTER);
  }

  // Final Stream Callback
  static size_t ogg_io_write(void *user_handle, void *buf, size_t n) {
    LOGD("ogg_io_write: %u", n);
    OggEncoder *self = (OggEncoder *)user_handle;
    self->p_print->write((uint8_t *)buf, n);
  }
};

}  // namespace audio_tools
