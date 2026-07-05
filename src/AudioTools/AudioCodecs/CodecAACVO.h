#pragma once

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "VOAACEncoder.h"

#ifndef VOAAC_DEFAULT_BITRATE
#define VOAAC_DEFAULT_BITRATE 128000
#endif

#ifndef VOAAC_DEFAULT_OUTPUT_BUFFER_SIZE
#define VOAAC_DEFAULT_OUTPUT_BUFFER_SIZE 2048
#endif

namespace audio_tools {

/**
 * @brief AAC Encoder based on the VisualOn vo-aacenc implementation
 * (https://github.com/pschatzmann/codec-vo-aacenc).
 *
 * The encoder consumes PCM in blocks of 1024 samples per channel, so this
 * class buffers arbitrary write sizes and encodes full frames automatically.
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AACEncoderVO : public AudioEncoder {
public:
  AACEncoderVO() = default;

  AACEncoderVO(Print &out_stream) { setOutput(out_stream); }

  ~AACEncoderVO() override { end(); }

  /// Defines the output stream
  void setOutput(Print &out_stream) override { p_print = &out_stream; }

  /// Defines target bitrate in bps
  void setBitrate(uint32_t bps) { bitrate = bps; }

  /// Defines if ADTS headers are generated
  void setAdts(bool enabled) { use_adts = enabled; }

  /// Defines max encoded output frame size
  void setOutputBufferSize(size_t len) { output_buffer_size = len; }

  bool begin(AudioInfo info) override {
    setAudioInfo(info);
    return begin();
  }

  bool begin() override {
    if (p_print == nullptr) {
      LOGE("Output undefined");
      return false;
    }
    if (info.channels < 1 || info.channels > 2) {
      LOGE("Invalid channels: %d", info.channels);
      return false;
    }
    if (info.bits_per_sample != 16) {
      LOGE("bits_per_sample must be 16, got %d", info.bits_per_sample);
      return false;
    }

    frame_bytes =
        VOAACEncoder::kFrameSamplesPerChannel * info.channels * sizeof(int16_t);
    pcm_buffer.resize(frame_bytes);
    out_buffer.resize(output_buffer_size);
    pcm_pos = 0;

    active = enc.begin(info.sample_rate, bitrate, info.channels, use_adts);
    if (!active) {
      LOGE("VO AAC encoder init failed");
      return false;
    }
    return true;
  }

  size_t write(const uint8_t *data, size_t len) override {
    if (!active || data == nullptr || len == 0)
      return 0;

    size_t processed = 0;
    while (processed < len) {
      size_t open = frame_bytes - pcm_pos;
      size_t to_copy = (len - processed) < open ? (len - processed) : open;
      memcpy(pcm_buffer.data() + pcm_pos, data + processed, to_copy);
      pcm_pos += to_copy;
      processed += to_copy;

      if (pcm_pos >= frame_bytes) {
        if (!encodeFrame()) {
          break;
        }
        pcm_pos = 0;
      }
    }
    return processed;
  }

  void end() override {
    if (!active) {
      return;
    }

    if (pcm_pos > 0) {
      // Flush remaining partial PCM frame padded with silence.
      memset(pcm_buffer.data() + pcm_pos, 0, frame_bytes - pcm_pos);
      encodeFrame();
      pcm_pos = 0;
    }

    enc.end();
    active = false;
  }

  const char *mime() override { return "audio/aac"; }

  operator bool() override { return active && enc.isReady(); }

  VOAACEncoder *driver() { return &enc; }

protected:
  VOAACEncoder enc;
  Print *p_print = nullptr;
  Vector<uint8_t> pcm_buffer{0};
  Vector<uint8_t> out_buffer{0};
  size_t frame_bytes = 0;
  size_t pcm_pos = 0;
  size_t output_buffer_size = VOAAC_DEFAULT_OUTPUT_BUFFER_SIZE;
  uint32_t bitrate = VOAAC_DEFAULT_BITRATE;
  bool use_adts = true;
  bool active = false;

  bool encodeFrame() {
    size_t out_bytes = 0;
    bool ok = enc.encodeFrame((const int16_t *)pcm_buffer.data(),
                              VOAACEncoder::kFrameSamplesPerChannel,
                              out_buffer.data(), out_buffer.size(), out_bytes);
    if (!ok) {
      LOGE("VO AAC encodeFrame failed");
      return false;
    }
    if (out_bytes > 0) {
      size_t written = p_print->write(out_buffer.data(), out_bytes);
      if (written != out_bytes) {
        LOGW("Output truncated: %d of %d", (int)written, (int)out_bytes);
      }
    }
    return true;
  }
};

} // namespace audio_tools