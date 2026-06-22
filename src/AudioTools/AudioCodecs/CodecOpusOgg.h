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
  Vector<uint8_t> pendingPacket{0};
  ogg_int64_t totalGranulepos = 0;
  ogg_int64_t samplesPerPacket = 960;
  ogg_int64_t finalPacketSamples = 0;
  bool hasPendingPacket = false;

  bool writeHeader() override {
    LOGI("writeHeader");
    bool result = true;
    totalGranulepos = 0;
    finalPacketSamples = 0;
    hasPendingPacket = false;
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
    oh1.b_o_s = false;
    oh1.e_o_s = false;
    if (!writePacket(oh1, OGGZ_FLUSH_AFTER)) {
      result = false;
      LOGE("writePacket-header1");
    }
    TRACED();
    return result;
  }

  bool emitBufferedPacket(ogg_int64_t granulePosition, bool endOfStream) {
    if (!hasPendingPacket) return true;

    op.packet = pendingPacket.data();
    op.bytes = pendingPacket.size();
    op.granulepos = granulePosition;
    op.b_o_s = false;
    op.e_o_s = endOfStream;
    op.packetno = packetno++;
    is_audio = true;
    if (!writePacket(op, OGGZ_FLUSH_AFTER)) {
      return false;
    }

    while ((oggz_write(p_oggz, op.bytes)) > 0)
      ;

    hasPendingPacket = false;
    pendingPacket.resize(0);
    return true;
  }

 public:
  void setPreSkipSamples(uint16_t preSkipSamples) {
    header.preSkip = preSkipSamples;
  }

  void setFrameDurationUs(uint32_t frameDurationUs) {
    ogg_int64_t opusSamples =
        (ogg_int64_t)(((uint64_t)frameDurationUs * 48000ULL) / 1000000ULL);
    samplesPerPacket = opusSamples > 0 ? opusSamples : 960;
  }

  void setFinalPacketSamples(ogg_int64_t samples) {
    finalPacketSamples = samples;
  }

  size_t write(const uint8_t *data, size_t len) override {
    if (data == nullptr) return 0;
    LOGD("OpusOggWriter::write: %d", (int)len);

    if (hasPendingPacket) {
      totalGranulepos += samplesPerPacket;
      if (!emitBufferedPacket(totalGranulepos, false)) {
        return 0;
      }
    }

    pendingPacket.resize(len);
    memcpy(pendingPacket.data(), data, len);
    hasPendingPacket = true;

    return len;
  }

  void end() override {
    TRACED();

    if (hasPendingPacket) {
      ogg_int64_t keptSamples =
          finalPacketSamples > 0 ? finalPacketSamples : samplesPerPacket;
      if (keptSamples > samplesPerPacket) {
        keptSamples = samplesPerPacket;
      }
      totalGranulepos += keptSamples;
      emitBufferedPacket(totalGranulepos, true);
    }

    is_open = false;
    oggz_close(p_oggz);
    p_oggz = nullptr;
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

  /// provides the frame duration in us (e.g. for RTSP)
  uint32_t frameDurationUs() override {
    // Get frame duration from encoder settings
    int frameDurationMs = config().frame_sizes_ms_x2;
    uint32_t frameDurationUs = 10000;
    switch (frameDurationMs) {
      case OPUS_FRAMESIZE_2_5_MS:
        frameDurationUs = 2500;
        break;
      case OPUS_FRAMESIZE_5_MS:
        frameDurationUs = 5000;
        break;
      case OPUS_FRAMESIZE_10_MS:
        frameDurationUs = 10000;
        break;
      case OPUS_FRAMESIZE_20_MS:
        frameDurationUs = 20000;
        break;
      case OPUS_FRAMESIZE_40_MS:
        frameDurationUs = 40000;
        break;
      case OPUS_FRAMESIZE_60_MS:
        frameDurationUs = 60000;
        break;
      case OPUS_FRAMESIZE_80_MS:
        frameDurationUs = 80000;
        break;
      case OPUS_FRAMESIZE_100_MS:
        frameDurationUs = 100000;
        break;
      case OPUS_FRAMESIZE_120_MS:
        frameDurationUs = 120000;
        break;
    }
    return frameDurationUs;
  }

  bool begin(AudioInfo from) override {
    setAudioInfo(from);
    return begin();
  }

  bool begin() override {
    if (p_codec == nullptr) return false;

    ogg_writer.setFrameDurationUs(frameDurationUs());
    p_codec->setOutput(*p_ogg);
    if (!p_codec->begin(p_ogg->audioInfo())) {
      return false;
    }

    int lookaheadSamples = enc.lookaheadSamples();
    if (lookaheadSamples > 0) {
      uint32_t preSkipSamples =
          (uint32_t)(((uint64_t)lookaheadSamples * 48000U) /
                     (uint32_t)enc.config().sample_rate);
      ogg_writer.setPreSkipSamples((uint16_t)preSkipSamples);
    }

    if (!p_ogg->begin()) {
      p_codec->end();
      return false;
    }

    return true;
  }

  void end() override {
    int finalSamples = enc.pendingSamples();
    if (finalSamples <= 0) {
      finalSamples = enc.frameSizeSamples();
    }

    ogg_int64_t finalGranuleSamples =
        (ogg_int64_t)(((uint64_t)finalSamples * 48000ULL) /
                      (uint32_t)enc.config().sample_rate);
    ogg_writer.setFinalPacketSamples(finalGranuleSamples);

    OggContainerEncoder::end();
  }

 protected:
  // use custom writer
  OpusOggWriter ogg_writer;
  // use opus encoder
  OpusAudioEncoder enc;
};

#include "AudioTools/Communication/RTSP/RTSPFormat.h"

}  // namespace audio_tools
