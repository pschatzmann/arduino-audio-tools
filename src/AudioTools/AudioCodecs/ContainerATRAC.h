/**
 * @file ContainerATRAC.h
 * @brief Containers for ATRAC encoded audio: the decoders parse the header of
 * the stream, configure a DecoderATRAC and forward the audio frames to it.
 *
 * - WAV (.at3, .at9) is handled by the WAVDecoder: register a DecoderATRAC
 *   for ATRAC3, ATRAC3PLUS and ATRAC9 with WAVDecoder::addDecoder()
 * - ContainerATRACOMA: Sony OpenMG audio (.oma, .aa3) with ATRAC3 and ATRAC3+
 * - ContainerATRACAEA: Sony MiniDisc Atrac1 (.aea)
 *
 * The data can be written in chunks of any size. Encrypted OMA files are not
 * supported.
 */
#pragma once

#include <string.h>

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/AudioCodecs/CodecATRAC.h"

namespace audio_tools {

/**
 * @brief Common functionality of the ATRAC container decoders: a streaming
 * header parser which does not need to buffer more than the header fields
 * that are needed to set up the DecoderATRAC.
 * @ingroup codecs
 * @author Phil Schatzmann
 */
class ContainerATRAC : public ContainerDecoder {
 public:
  using AudioDecoder::setOutput;

  /// Defines where the decoded PCM data is written to
  void setOutput(Print &out_stream) override {
    AudioDecoder::setOutput(out_stream);
    dec.setOutput(out_stream);
  }

  /// Prepares the decoder for a new stream
  bool begin() override {
    dec.end();
    hdr.clear();
    state = 0;
    skip_count = 0;
    payload_left = 0;
    is_streaming = false;
    is_done = false;
    is_failed = false;
    is_active = true;
    return true;
  }

  void end() override {
    dec.end();
    is_active = false;
  }

  operator bool() override { return is_active && !is_failed; }

  /// Provides access to the ATRAC decoder
  DecoderATRAC &decoder() { return dec; }

  /// Processes the container data: returns 0 if the stream is not supported
  size_t write(const uint8_t *data, size_t len) override {
    if (!is_active || is_failed) return 0;
    const uint8_t *p = data;
    size_t n = len;
    while (n > 0 && !is_failed && !is_done) {
      if (skip_count > 0) {
        size_t s = skip_count < n ? skip_count : n;
        p += s;
        n -= s;
        skip_count -= s;
      } else if (is_streaming) {
        writePayload(p, n);
      } else {
        int state_before = state;
        size_t n_before = n;
        parse(p, n);
        if (!is_failed && state == state_before && n == n_before &&
            !is_streaming) {
          LOGE("ATRAC header parser is not progressing");
          is_failed = true;
        }
      }
    }
    return is_failed ? 0 : len;
  }

 protected:
  DecoderATRAC dec;
  Vector<uint8_t> hdr;
  size_t skip_count = 0;
  /// remaining audio bytes (only relevant if payload_known)
  uint32_t payload_left = 0;
  int state = 0;
  bool payload_known = false;
  bool is_streaming = false;
  bool is_done = false;
  bool is_failed = false;
  bool is_active = false;

  /// Processes the header data: must consume data, change the state or fail
  virtual void parse(const uint8_t *&p, size_t &n) = 0;

  /// Copies the data into hdr until it contains need bytes
  bool collect(const uint8_t *&p, size_t &n, size_t need) {
    while ((size_t)hdr.size() < need && n > 0) {
      uint8_t value = *p;
      hdr.push_back(value);
      p++;
      n--;
    }
    return (size_t)hdr.size() >= need;
  }

  void fail(const char *msg) {
    LOGE("%s", msg);
    is_failed = true;
  }

  /// Configures the decoder: the audio data starts after the header
  bool startDecoder(ATRACType type, int sample_rate, int channels,
                    int block_align, const uint8_t *extra = nullptr,
                    size_t extra_size = 0, bool known = false,
                    uint32_t length = 0) {
    if (!dec.begin(type, sample_rate, channels, block_align, extra,
                   extra_size)) {
      fail("ATRAC decoder could not be started");
      return false;
    }
    setAudioInfo(dec.audioInfo());
    payload_known = known;
    payload_left = length;
    is_streaming = true;
    if (known && length == 0) is_done = true;
    return true;
  }

  void writePayload(const uint8_t *&p, size_t &n) {
    size_t chunk = n;
    if (payload_known && chunk > payload_left) chunk = payload_left;
    if (chunk > 0) dec.write(p, chunk);
    p += chunk;
    n -= chunk;
    if (payload_known) {
      payload_left -= chunk;
      // ignore everything which follows the audio data (e.g. other chunks)
      if (payload_left == 0) is_done = true;
    }
  }

  static uint16_t be16(const uint8_t *p) { return (p[0] << 8) | p[1]; }
  static uint32_t be24(const uint8_t *p) {
    return (p[0] << 16) | (p[1] << 8) | p[2];
  }
};

/**
 * @brief Decodes ATRAC3 and ATRAC3+ audio in a Sony OpenMG audio container
 * (.oma, .aa3, .at3): the file starts with an ID3v2 like tag ('ea3' or 'ID3')
 * which is followed by a 96 byte 'EA3' header with the codec parameters.
 * Encrypted files are not supported.
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 */
class ContainerATRACOMA : public ContainerATRAC {
 public:
  const char *mime() override { return "audio/oma"; }

 protected:
  enum State { Start = 0, Ea3 };
  static const size_t ID3_HEADER_SIZE = 10;
  static const size_t EA3_HEADER_SIZE = 96;
  enum CodecId { CodecATRAC3 = 0, CodecATRAC3Plus = 1 };

  void parse(const uint8_t *&p, size_t &n) override {
    switch (state) {
      case Start:
        if (!collect(p, n, ID3_HEADER_SIZE)) return;
        if (memcmp(hdr.data(), "ea3", 3) == 0 ||
            memcmp(hdr.data(), "ID3", 3) == 0) {
          size_t tag_size = syncSafe(hdr.data() + 6);
          // footer
          if (hdr.data()[5] & 0x10) tag_size += 10;
          skip_count = tag_size;
          hdr.clear();
        } else if (memcmp(hdr.data(), "EA3", 3) != 0) {
          fail("Not an OMA stream");
          return;
        }
        // an EA3 header without the tag is completed in the next state
        state = Ea3;
        break;

      case Ea3:
        if (!collect(p, n, EA3_HEADER_SIZE)) return;
        parseEa3();
        break;
    }
  }

  static size_t syncSafe(const uint8_t *p) {
    return ((p[0] & 0x7F) << 21) | ((p[1] & 0x7F) << 14) |
           ((p[2] & 0x7F) << 7) | (p[3] & 0x7F);
  }

  void parseEa3() {
    const uint8_t *h = hdr.data();
    if (memcmp(h, "EA3", 3) != 0 || h[4] != 0 || h[5] != EA3_HEADER_SIZE) {
      fail("OMA: EA3 header not found");
      return;
    }
    // 0xFFFF and 0xFF80: not encrypted
    uint16_t encryption_id = be16(h + 6);
    if (encryption_id != 0xFFFF && encryption_id != 0xFF80) {
      fail("OMA: encrypted files are not supported");
      return;
    }
    uint8_t codec_id = h[32];
    uint32_t params = be24(h + 33);
    static const int rates[] = {32000, 44100, 48000, 88200, 96000};
    unsigned rate_idx = (params >> 13) & 7;
    if (rate_idx >= sizeof(rates) / sizeof(rates[0])) {
      fail("OMA: invalid sample rate");
      return;
    }
    int sample_rate = rates[rate_idx];
    int frame_size = (params & 0x3FF) * 8;

    if (codec_id == CodecATRAC3) {
      // joint stereo flag: fake the extradata of the WAV format
      uint16_t js = (params >> 17) & 1;
      uint8_t extra[14] = {1, 0, 0, 0x10, 0, 0, (uint8_t)js, 0,
                           (uint8_t)js, 0, 1, 0, 0, 0};
      hdr.clear();
      startDecoder(ATRAC3, sample_rate, 2, frame_size, extra, sizeof(extra));
    } else if (codec_id == CodecATRAC3Plus) {
      static const int channel_ids[] = {0, 1, 2, 3, 4, 6, 7, 8};
      int channels = channel_ids[(params >> 10) & 7];
      if (channels == 0) {
        fail("OMA: invalid channel configuration");
        return;
      }
      hdr.clear();
      startDecoder(ATRAC3PLUS, sample_rate, channels, frame_size + 8);
    } else {
      fail("OMA: unsupported codec (only ATRAC3 and ATRAC3+)");
    }
  }
};

/**
 * @brief Decodes ATRAC1 audio of a Sony MiniDisc .aea file: a 2048 byte header
 * (magic, title, number of channels) is followed by the frames of 212 bytes
 * per channel. The sample rate is 44100.
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 */
class ContainerATRACAEA : public ContainerATRAC {
 public:
  const char *mime() override { return "audio/aea"; }

 protected:
  static const size_t HEADER_SIZE = 2048;
  static const size_t CHANNELS_POS = 264;
  static const int SOUND_UNIT_SIZE = 212;

  void parse(const uint8_t *&p, size_t &n) override {
    if (!collect(p, n, CHANNELS_POS + 1)) return;
    static const uint8_t magic[4] = {0x00, 0x08, 0x00, 0x00};
    if (memcmp(hdr.data(), magic, 4) != 0) {
      fail("Not an AEA stream");
      return;
    }
    int channels = hdr.data()[CHANNELS_POS];
    hdr.clear();
    if (channels != 1 && channels != 2) {
      fail("AEA: invalid number of channels");
      return;
    }
    startDecoder(ATRAC1, 44100, channels, SOUND_UNIT_SIZE * channels);
    // the rest of the header is not needed
    skip_count = HEADER_SIZE - (CHANNELS_POS + 1);
  }
};

}  // namespace audio_tools
