#pragma once

#include "AudioCodecs/ContainerOgg.h"
#include "AudioCodecs/CodecOpus.h"

namespace audio_tools {

/// Opus header
struct __attribute__((packed)) OpusOggHeader {
  char signature[8] = {'O', 'p', 'u', 's', 'H', 'e', 'a', 'd'};
  uint8_t version = 1;
  uint8_t channelCount = 2;
  uint16_t preSkip = 3840;
  uint32_t sampleRate = 48000;
  int16_t outputGain = 0;
  uint8_t channelMappingFamily = 0;
};

/// Simplified header w/o comments
struct __attribute__((packed)) OpusOggCommentHeader {
  char signature[8] = {'O', 'p', 'u', 's', 'T', 'a', 'g', 's'};
  uint32_t vendorStringLength = 8;
  char vendor[8] = "Arduino";
  uint32_t userCommentListLength = 0;
};

/**
 * @brief Opus Decoder which uses the Ogg Container. See
 * https://datatracker.ietf.org/doc/html/rfc7845. The audio data is transmitted in frames and the
 * header information contains the sampler rate, channels and other critical info.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class OpusOggDecoder : public OggContainerDecoder {
 public:
  OpusOggDecoder() = default;

  /// Defines the output Stream
  void setOutputStream(Print &out_stream) override {
    LOGD(LOG_METHOD);
    dec.setOutputStream(out_stream);
    p_print = &opus;
  }

  /// Provides access to the Opus configuration
  OpusSettings &config()  { return dec.config(); }

  void begin() override {
    LOGD(LOG_METHOD);
    OggContainerDecoder::begin();
    dec.begin();
  }

  void end() override {
    LOGD(LOG_METHOD);
    OggContainerDecoder::begin();
    dec.end();
  }

 protected:
  OpusOggHeader header;
  OpusAudioDecoder dec;
  EncodedAudioStream opus{(Print *)nullptr, &dec};

  virtual void beginOfSegment(ogg_packet *op) {
    LOGD("bos");
    if (strncmp((char *)op->bytes, "OpusHead", 8) == 0) {
      memmove(&header, (char *)op->packet, sizeof(header));
      cfg.sample_rate = header.sampleRate;
      cfg.channels = header.channelCount;
      notify();
    } else if (strncmp((char *)op->bytes, "OpusTags", 8) == 0) {
      // not processed
    }
  }
};

/**
 * @brief Opus Encoder which uses the Ogg Container: see
 * https://datatracker.ietf.org/doc/html/rfc7845
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class OpusOggEncoder : public OggContainerEncoder {
 public:
  OpusOggEncoder() = default;

  /// Defines the output Stream
  void setOutputStream(Print &out_stream) override {
    LOGD(LOG_METHOD);
    enc.setOutputStream(out_stream);
    p_print = &opus;
  }

  void begin() override {
    LOGD(LOG_METHOD);
    OggContainerEncoder::begin();
    enc.begin();
  }

  void end() override {
    LOGD(LOG_METHOD);
    OggContainerEncoder::begin();
    enc.end();
  }

  /// Provides "audio/opus"
  const char *mime() override { return "audio/opus"; }

  /// Provides access to the Opus configuration
  OpusEncoderSettings &config() { return enc.config(); }

 protected:
  OpusOggHeader header;
  OpusOggCommentHeader comment;
  OpusAudioEncoder enc;
  EncodedAudioStream opus{(Print *)nullptr, &enc};
  ogg_packet oh1;

  bool writeHeader() override {
    LOGI("writeHeader");
    bool result = true;
    header.sampleRate = cfg.sample_rate;
    header.channelCount = cfg.channels;
    // write header
    oh.packet = (uint8_t *)&header;
    oh.bytes = sizeof(header);
    oh.granulepos = 0;
    oh.packetno = packetno++;
    oh.b_o_s = true;
    oh.e_o_s = false;
    if (!writePacket(oh)){
      result = false;
    }

    // write comment header
    oh1.packet = (uint8_t *)&comment;
    oh1.bytes = sizeof(comment);
    oh1.granulepos = 0;
    oh1.packetno = packetno++;
    oh1.b_o_s = true;
    oh1.e_o_s = false;
    if (!writePacket(oh1, OGGZ_FLUSH_AFTER)){
      result = false;
    }
    LOGD(LOG_METHOD);
    return result;
  }
};

}  // namespace audio_tools