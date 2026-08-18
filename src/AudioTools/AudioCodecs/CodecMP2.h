#pragma once

#include <string.h>

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "TinyMP2.h"

namespace audio_tools {

/**
 * @brief Decoder for MPEG-1 Layer II (MP2) audio using the TinyMP2 library.
 * Compressed data provided via write() does not need to be aligned on frame
 * boundaries: bytes are accumulated internally and decoded frame by frame;
 * any invalid/garbage bytes are skipped automatically (resync).
 *
 * Depends on https://github.com/pschatzmann/TinyMP2
 *
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MP2Decoder : public AudioDecoder {
 public:
  MP2Decoder() = default;

  ~MP2Decoder() {
    if (active) end();
  }

  /// Defines the output Stream
  void setOutput(Print &out_stream) override { p_print = &out_stream; }

  bool begin() override {
    TRACED();
    decoder.reset();
    buffer.resize(0);
    pcm.resize(TinyMP2Decoder::kSamplesPerFrame * TinyMP2Decoder::kMaxChannels);
    active = true;
    return true;
  }

  void end() override {
    TRACED();
    buffer.resize(0);
    active = false;
  }

  /// Provide compressed MP2 data: no frame alignment required
  size_t write(const uint8_t *data, size_t len) override {
    if (!active || len == 0) return 0;
    size_t old_size = buffer.size();
    buffer.resize(old_size + len);
    memcpy(buffer.data() + old_size, data, len);
    decode();
    return len;
  }

  operator bool() override { return active; }

 protected:
  Print *p_print = nullptr;
  TinyMP2Decoder decoder;
  Vector<uint8_t> buffer{0};
  Vector<int16_t> pcm{0};
  bool active = false;

  /// Decodes as many complete frames as are available in buffer; leading
  /// garbage is skipped one byte at a time, trailing incomplete data is kept
  /// for the next write()
  void decode() {
    size_t pos = 0;
    while (pos < buffer.size()) {
      TinyMP2Decoder::FrameHeader header;
      size_t consumed = decoder.decodeFrame(
          buffer.data() + pos, buffer.size() - pos, pcm.data(), &header);
      if (consumed == 0) {
        TinyMP2Decoder::FrameHeader peek;
        if (decoder.parseHeader(buffer.data() + pos, buffer.size() - pos,
                                 peek) &&
            buffer.size() - pos < peek.frame_bytes) {
          // valid header, but the frame is not fully buffered yet
          break;
        }
        // no valid sync at this position: resync by advancing a single byte
        pos++;
        continue;
      }
      provideResult(header);
      pos += consumed;
    }
    // keep unprocessed trailing bytes for the next write()
    size_t remaining = buffer.size() - pos;
    if (pos > 0) {
      memmove(buffer.data(), buffer.data() + pos, remaining);
    }
    buffer.resize(remaining);
  }

  /// Notifies about audio info changes and writes the decoded PCM data
  void provideResult(TinyMP2Decoder::FrameHeader &header) {
    AudioInfo tmp;
    tmp.sample_rate = header.sample_rate;
    tmp.channels = header.channels;
    tmp.bits_per_sample = 16;
    if (tmp != info) {
      info = tmp;
      notifyAudioChange(tmp);
    }
    if (p_print != nullptr) {
      size_t bytes =
          TinyMP2Decoder::kSamplesPerFrame * header.channels * sizeof(int16_t);
      p_print->write((const uint8_t *)pcm.data(), bytes);
    }
  }
};

}  // namespace audio_tools
