#pragma once

#include "AudioCodecs/AudioCodecsBase.h"
#include "AudioCodecs/CodecOpus.h"
#include "AudioTools/Buffers.h"
#include "oggz/oggz.h"

#define OGG_READ_SIZE (1024)
#define OGG_DEFAULT_BUFFER_SIZE (OGG_READ_SIZE)
// #define OGG_DEFAULT_BUFFER_SIZE (246)
// #define OGG_READ_SIZE (512)

namespace audio_tools {

/**
 * @brief Decoder for Ogg Container. Decodes a packet from an Ogg
 * container. The Ogg begin segment contains the AudioInfo structure. You can
 * subclass and overwrite the beginOfSegment() method to implement your own
 * headers
 * Dependency: https://github.com/pschatzmann/arduino-libopus
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class OggContainerDecoder : public ContainerDecoder {
 public:
  /**
   * @brief Construct a new OggContainerDecoder object
   */

  OggContainerDecoder() {
    p_codec = &dec_copy;
    out.setDecoder(p_codec);
  }

  OggContainerDecoder(AudioDecoder *decoder) { setDecoder(decoder); }

  OggContainerDecoder(AudioDecoder &decoder) { setDecoder(&decoder); }

  void setDecoder(AudioDecoder *decoder) {
    p_codec = decoder;
    out.setDecoder(p_codec);
  }

  /// Defines the output Stream
  void setOutput(Print &print) override { out.setOutput(&print); }

  void addNotifyAudioChange(AudioInfoSupport &bi) override {
    out.addNotifyAudioChange(bi);
    ContainerDecoder::addNotifyAudioChange(bi);
  }

  AudioInfo audioInfo() override { return out.audioInfo(); }

  bool begin(AudioInfo info) override {
    TRACED();
    this->info = info;
    return begin();
  }

  bool begin() override {
    TRACED();
    out.setAudioInfo(info);
    out.begin();
    if (p_oggz == nullptr) {
      p_oggz = oggz_new(OGGZ_READ | OGGZ_AUTO);  // OGGZ_NONSTRICT
      is_open = true;
      // Callback to Replace standard IO
      if (oggz_io_set_read(p_oggz, ogg_io_read, this) != 0) {
        LOGE("oggz_io_set_read");
        is_open = false;
      }
      // Callback
      if (oggz_set_read_callback(p_oggz, -1, read_packet, this) != 0) {
        LOGE("oggz_set_read_callback");
        is_open = false;
      }

      if (oggz_set_read_page(p_oggz, -1, read_page, this) != 0) {
        LOGE("oggz_set_read_page");
        is_open = false;
      }
    }
    return is_open;
  }

  void end() override {
    TRACED();
    flush();
    out.end();
    is_open = false;
    oggz_close(p_oggz);
    p_oggz = nullptr;
  }

  void flush() {
    LOGD("oggz_read...");
    while ((oggz_read(p_oggz, OGG_READ_SIZE)) > 0)
      ;
  }

  virtual size_t write(const uint8_t *data, size_t len) override {
    LOGD("write: %d", (int)len);

    // fill buffer
    size_t size_consumed = buffer.writeArray((uint8_t *)data, len);
    if (buffer.availableForWrite() == 0) {
      // Read all bytes into oggz, calling any read callbacks on the fly.
      flush();
    }
    // write remaining bytes
    if (size_consumed < len) {
      size_consumed += buffer.writeArray((uint8_t *)data + size_consumed,
                                         len - size_consumed);
      flush();
    }
    return size_consumed;
  }

  virtual operator bool() override { return is_open; }

 protected:
  EncodedAudioOutput out;
  CopyDecoder dec_copy;
  AudioDecoder *p_codec = nullptr;
  RingBuffer<uint8_t> buffer{OGG_DEFAULT_BUFFER_SIZE};
  OGGZ *p_oggz = nullptr;
  bool is_open = false;
  long pos = 0;

  // Final Stream Callback -> provide data to ogg
  static size_t ogg_io_read(void *user_handle, void *buf, size_t n) {
    LOGD("ogg_io_read: %d", (int)n);
    size_t result = 0;
    OggContainerDecoder *self = (OggContainerDecoder *)user_handle;
    if (self->buffer.available() >= n) {
      OggContainerDecoder *self = (OggContainerDecoder *)user_handle;
      result = self->buffer.readArray((uint8_t *)buf, n);
      self->pos += result;

    } else {
      result = 0;
    }
    return result;
  }

  // Process full packet
  static int read_packet(OGGZ *oggz, oggz_packet *zp, long serialno,
                         void *user_data) {
    LOGD("read_packet: %d", (int)zp->op.bytes);
    OggContainerDecoder *self = (OggContainerDecoder *)user_data;
    ogg_packet *op = &zp->op;
    int result = op->bytes;
    if (op->b_o_s) {
      self->beginOfSegment(op);
    } else if (op->e_o_s) {
      self->endOfSegment(op);
    } else {
      if (memcmp(op->packet, "OpusTags", 8) == 0) {
        self->beginOfSegment(op);
      } else {
        LOGD("process audio packet");
        int eff = self->out.write(op->packet, op->bytes);
        if (eff != result) {
          LOGE("Incomplere write");
        }
      }
    }
    // 0 = success
    return 0;
  }

  static int read_page(OGGZ *oggz, const ogg_page *og, long serialno,
                       void *user_data) {
    LOGD("read_page: %d", (int)og->body_len);
    // 0 = success
    return 0;
  }

  virtual void beginOfSegment(ogg_packet *op) {
    LOGD("bos");
    if (op->bytes == sizeof(AudioInfo)) {
      AudioInfo cfg(*(AudioInfo*)op->packet);
      cfg.logInfo();
      if (cfg.bits_per_sample == 16 || cfg.bits_per_sample == 24 ||
          cfg.bits_per_sample == 32) {
        setAudioInfo(cfg);
      } else {
        LOGE("Invalid AudioInfo")
      }
    } else {
      LOGE("Invalid Header")
    }
  }

  virtual void endOfSegment(ogg_packet *op) {
    // end segment not supported
    LOGW("e_o_s");
  }
};

/**
 * @brief Output class for the OggContainerEncoder. Each
 * write is ending up as container entry
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class OggContainerOutput : public AudioOutput {
 public:
  // Empty Constructor - the output stream must be provided with begin()
  OggContainerOutput() = default;

  /// Defines the output Stream
  void setOutput(Print &print) { p_out = &print; }

  /// starts the processing using the actual AudioInfo
  virtual bool begin() override {
    TRACED();
    assert(cfg.channels != 0);
    assert(cfg.sample_rate != 0);
    is_open = true;
    if (p_oggz == nullptr) {
      p_oggz = oggz_new(OGGZ_WRITE | OGGZ_NONSTRICT | OGGZ_AUTO);
      serialno = oggz_serialno_new(p_oggz);
      oggz_io_set_write(p_oggz, ogg_io_write, this);
      packetno = 0;
      granulepos = 0;

      if (!writeHeader()) {
        is_open = false;
        LOGE("writeHeader");
      }
    }
    return is_open;
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
  virtual size_t write(const uint8_t *data, size_t len) override {
    if (data == nullptr) return 0;
    LOGD("OggContainerOutput::write: %d", (int)len);
    assert(cfg.channels != 0);

    // encode the data
    op.packet = (uint8_t *)data;
    op.bytes = len;
    if (op.bytes > 0) {
      int bytes_per_sample = cfg.bits_per_sample / 8;
      granulepos += op.bytes / bytes_per_sample;  // sample
      op.granulepos = granulepos;
      op.b_o_s = false;
      op.e_o_s = false;
      op.packetno = packetno++;
      is_audio = true;
      if (!writePacket(op, OGGZ_FLUSH_AFTER)) {
        return 0;
      }
    }
    // trigger pysical write
    while ((oggz_write(p_oggz, len)) > 0)
      ;

    return len;
  }
  bool isOpen() { return is_open; }

 protected:
  Print *p_out = nullptr;
  bool is_open = false;
  OGGZ *p_oggz = nullptr;
  ogg_packet op;
  ogg_packet oh;
  size_t granulepos = 0;
  size_t packetno = 0;
  long serialno = -1;
  bool is_audio = false;

  virtual bool writePacket(ogg_packet &op, int flag = 0) {
    LOGD("writePacket: %d", (int)op.bytes);
    long result = oggz_write_feed(p_oggz, &op, serialno, flag, NULL);
    if (result < 0 && result != OGGZ_ERR_OUT_OF_MEMORY) {
      LOGE("oggz_write_feed: %d", (int)result);
      return false;
    }
    return true;
  }

  virtual bool writeHeader() {
    TRACED();
    oh.packet = (uint8_t *)&cfg;
    oh.bytes = sizeof(AudioInfo);
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
    LOGD("ogg_io_write: %d", (int)n);
    OggContainerOutput *self = (OggContainerOutput *)user_handle;
    if (self == nullptr) {
      LOGE("self is null");
      return 0;
    }
    // self->out.write((uint8_t *)buf, n);
    writeSamples<uint8_t>(self->p_out, (uint8_t *)buf, n);
    // 0 = continue
    return 0;
  }
};

/**
 * @brief Encoder for Ogg Container. Encodes a packet for an Ogg
 * container. The Ogg begin segment contains the AudioInfo structure. You can
 * subclass ond overwrite the writeHeader() method to implement your own header
 * logic. When an optional encoder is specified in the constructor we package
 * the encoded data.
 * Dependency: https://github.com/pschatzmann/arduino-libopus
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class OggContainerEncoder : public AudioEncoder {
 public:
  // Empty Constructor - the output stream must be provided with begin()
  OggContainerEncoder() = default;

  OggContainerEncoder(AudioEncoder *encoder) { setEncoder(encoder); }

  OggContainerEncoder(AudioEncoder &encoder) { setEncoder(&encoder); }

  /// Defines the output Stream
  void setOutput(Print &print) override { p_ogg->setOutput(print); }

  /// Provides "audio/pcm"
  const char *mime() override { return mime_pcm; }

  /// We actually do nothing with this
  virtual void setAudioInfo(AudioInfo info) override {
    AudioEncoder::setAudioInfo(info);
    p_ogg->setAudioInfo(info);
    if (p_codec != nullptr) p_codec->setAudioInfo(info);
  }

  virtual bool begin(AudioInfo from) override {
    setAudioInfo(from);
    return begin();
  }

  /// starts the processing using the actual AudioInfo
  virtual bool begin() override {
    TRACED();
    p_ogg->begin();
    if (p_codec==nullptr) return false;
    p_codec->setOutput(*p_ogg);
    return p_codec->begin(p_ogg->audioInfo());
  }

  /// stops the processing
  void end() override {
    TRACED();
    if (p_codec != nullptr) p_codec->end();
    p_ogg->end();
  }

  /// Writes raw data to be encoded and packaged
  virtual size_t write(const uint8_t *data, size_t len) override {
    if (!p_ogg->isOpen() || data == nullptr) return 0;
    LOGD("OggContainerEncoder::write: %d", (int)len);
    size_t result = 0;
    if (p_codec == nullptr) {
      result = p_ogg->write((const uint8_t *)data, len);
    } else {
      result = p_codec->write(data, len);
    }
    return result;
  }

  operator bool() override { return p_ogg->isOpen(); }

  bool isOpen() { return p_ogg->isOpen(); }

 protected:
  AudioEncoder *p_codec = nullptr;
  OggContainerOutput ogg;
  OggContainerOutput *p_ogg = &ogg;

  void setEncoder(AudioEncoder *enc) { p_codec = enc; }

  /// Replace the ogg output class
  void setOggOutput(OggContainerOutput *out) { p_ogg = out; }
};

}  // namespace audio_tools
