#pragma once

#include "AudioTools/AudioCodecs/CodecOpus.h"
#include "AudioTools/AudioCodecs/ContainerOgg.h"

namespace audio_tools {

/// Opus header
struct __attribute__((packed)) OpusOggHeader {
  char signature[8] = {'O', 'p', 'u', 's', 'H', 'e', 'a', 'd'};
  uint8_t version = 1;
  uint8_t channelCount = 0;
  uint16_t preSkip = 3840;
  uint32_t sampleRate = 0;
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
 * https://datatracker.ietf.org/doc/html/rfc7845. The audio data is transmitted
 * in frames and the header information contains the sampler rate, channels and
 * other critical info.
 * Dependency: https://github.com/pschatzmann/arduino-libopus
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class OpusOggDecoder : public OggContainerDecoder {
 public:
  OpusOggDecoder() {
    p_codec = &dec;  // OpusAudioDecoder
    out.setDecoder(p_codec);
  };


  /// Provides access to the Opus configuration
  OpusSettings &config() { return dec.config(); }

  bool begin(OpusSettings settings) {
    OggContainerDecoder::begin();
    return dec.begin(settings);
  }

  bool begin() override {
    TRACED();
    OggContainerDecoder::begin();
    return dec.begin();
  }

  void end() override {
    TRACED();
    OggContainerDecoder::end();
    dec.end();
  }

 protected:
  OpusOggHeader header;
  OpusAudioDecoder dec;

  virtual void beginOfSegment(ogg_packet *op) override {
    LOGD("bos");
    if (op->packet == nullptr) return;
    if (strncmp("OpusHead", (char *)op->packet, 8) == 0) {
      memmove(&header, (char *)op->packet, sizeof(header));
      AudioInfo info = audioInfo();
      info.sample_rate = header.sampleRate;
      info.channels = header.channelCount;
      info.bits_per_sample = 16;
      info.logInfo();
      setAudioInfo(info);
    } else if (strncmp("OpusTags", (char *)op->packet, 8) == 0) {
      // not processed
    }
  }
};

class OpusOggWriter : public OggContainerOutput {

 protected:
  OpusOggHeader header;
  OpusOggCommentHeader comment;
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
    if (!writePacket(oh)) {
      result = false;
      LOGE("writePacket-header");
    }

    // write comment header
    oh1.packet = (uint8_t *)&comment;
    oh1.bytes = sizeof(comment);
    oh1.granulepos = 0;
    oh1.packetno = packetno++;
    oh1.b_o_s = true;
    oh1.e_o_s = false;
    if (!writePacket(oh1, OGGZ_FLUSH_AFTER)) {
      result = false;
      LOGE("writePacket-header1");
    }
    TRACED();
    return result;
  }
};

/**
 * @brief Opus Encoder which uses the Ogg Container: see
 * https://datatracker.ietf.org/doc/html/rfc7845
 * Dependency: https://github.com/pschatzmann/arduino-libopus
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class OpusOggEncoder : public OggContainerEncoder {
 public:
  OpusOggEncoder() {
    setOggOutput(&ogg_writer);
    setEncoder(&enc);
  }


  /// Provides "audio/opus"
  const char *mime() override { return "audio/ogg;codecs=opus"; }

  /// Provides access to the Opus config
  OpusEncoderSettings &config() { return enc.config(); }

 protected:
  // use custom writer
  OpusOggWriter ogg_writer;
  // use opus encoder
  OpusAudioEncoder enc;
};

}  // namespace audio_tools