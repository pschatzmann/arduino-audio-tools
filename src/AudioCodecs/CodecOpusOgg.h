#pragma once

#include "AudioCodecs/CodecOgg.h"
#include "AudioCodecs/CodecOpus.h"

namespace audio_tools {

/// Opus header
struct __attribute__((packed)) OpusOggHeader {
  char signature[8] = {'O','p','u','s','H','e','a','d'};
  uint8_t version = 1;
  uint8_t channelCount = 2;
  uint16_t preSkip = 3840;
  uint32_t sample_rate = 48000;
  int16_t output_gain = 0;
  uint8_t channelMappingFamily = 0;
};

/// Simplified header w/o comments
struct __attribute__((packed)) OpusOggCommentHeader {
  char signature[8] = {'O','p','u','s','T','a','g','s'};
  uint32_t vendorStringLength = 8;
  char vendor[8] = "Arduino";
  uint32_t userCommentListLength = 0;
};

/**
 * @brief Opus Decoder which uses the Ogg Container. See https://datatracker.ietf.org/doc/html/rfc7845
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class OpusOggDecoder : public OggDecoder {
 public:
  OpusOggDecoder() = default;

  /// Defines the output Stream
  void setOutputStream(Print &out_stream) override {
    dec.setOutputStream(out_stream);
    p_print = &opus;
  }

  /// Provides access to the Opus configuration  
  OpusSettings &config() { return dec.config(); }

  void begin() {
      OggDecoder::begin();
      dec.begin();
  }

  void end() {
      OggDecoder::begin();
      dec.end();
  }

 protected:
  OpusOggHeader header;
  OpusAudioDecoder dec;
  EncodedAudioStream opus{(Print*)nullptr, &dec};

  virtual void beginOfSegment(ogg_packet *op) {
    LOGD("bos");
    if (strncmp((char*)op->bytes, "OpusHead", 8) == 0) {
      memmove(&header, (char*)op->packet, sizeof(header));
      cfg.sample_rate = header.sample_rate;
      cfg.channels = header.channelCount;
      notify();
    } else if (strncmp((char*)op->bytes, "OpusTags", 8) == 0) {
      // not processed
    }
  }
};

/**
 * @brief Opus Encoder which uses the Ogg Container: see https://datatracker.ietf.org/doc/html/rfc7845
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class OpusOggEncoder : public OggEncoder {
 public:
  OpusOggEncoder() = default;

  /// Defines the output Stream
  void setOutputStream(Print &out_stream) override {
    enc.setOutputStream(out_stream);
    p_print = &opus;
  }

  void begin() {
      OggEncoder::begin();
      enc.begin();
  }

  void end() {
      OggEncoder::begin();
      enc.end();
  }

  /// Provides "audio/pcm"
  const char *mime() { return "audio/opus"; }

  /// Provides access to the Opus configuration
  OpusEncoderSettings &config() { return enc.config(); }


 protected:
  OpusOggHeader header;
  OpusOggCommentHeader comment;
  OpusAudioEncoder enc;
  EncodedAudioStream opus{(Print*)nullptr, &enc};

  void writeHeader() {
    header.sample_rate = cfg.sample_rate;
    header.channelCount = cfg.channels;
    // write header
    op.packet = (uint8_t *)&header;
    op.bytes = sizeof(header);
    op.granulepos = 0;
    op.packetno = packetno++;
    op.b_o_s = true;
    op.e_o_s = false;
    writePacket(op.bytes);

    // write comment header
    op.packet = (uint8_t *)&comment;
    op.bytes = sizeof(comment);
    op.granulepos = 0;
    op.packetno = packetno++;
    op.b_o_s = true;
    op.e_o_s = false;
    writePacket(op.bytes);
  }
};

}  // namespace audio_tools