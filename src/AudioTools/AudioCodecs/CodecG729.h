/**
 * @file CodecG729.h
 * @author Phil Schatzmann
 * @brief G.729 Codec – encoder and decoder wrapping the bcg729 library.
 * @version 0.2
 * @date 2026-05-07
 * @copyright GPLv3
 */

#pragma once

#include <bcg729.h>  // https://github.com/pschatzmann/codec-bcg729

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"

namespace audio_tools {

// G.729 frame sizes
static constexpr size_t G729_SAMPLES_PER_FRAME = 80;  // 10 ms @ 8 kHz
static constexpr size_t G729_PCM_SIZE =
    G729_SAMPLES_PER_FRAME * 2;              // 160 bytes PCM16
static constexpr size_t G729_ENC_SIZE = 10;  // 10 bytes speech frame
static constexpr size_t G729_SID_SIZE = 2;   // 2 bytes SID frame
static AudioInfo audioInfoG729{8000, 1, 16};

/**
 * @brief G.729 decoder using the bcg729 library.
 *
 * Accepts a raw G.729 byte stream and writes 16-bit PCM samples to the
 * configured output.  Both 10-byte speech frames and 2-byte SID
 * (comfort-noise) frames are supported:
 *  - When `write()` is called with exactly 10 or 2 bytes and the internal
 *    accumulator is empty, the frame is decoded immediately (framed mode).
 *  - Otherwise bytes are accumulated until a full 10-byte speech frame is
 *    available (stream mode).  SID frames must be delivered in framed mode
 *    to preserve their 2-byte boundary.
 *
 * Output is always fixed at 8 000 Hz / mono / 16-bit PCM.  Passing any
 * other AudioInfo via setAudioInfo() triggers a warning but the fixed
 * format is still used.
 *
 * @note Requires https://github.com/pschatzmann/codec-bcg729
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class G729Decoder : public AudioDecoder {
 public:
  G729Decoder() { AudioDecoder::setAudioInfo(audioInfoG729); }

  void setAudioInfo(AudioInfo info) override {
    // Check if requested format is valid
    if (info != audioInfoG729) {
      LOGE(
          "G.729 encoder input is fixed to 8000 Hz, mono, 16-bit PCM "
          "(requested: %d Hz, %d ch, %d bit)",
          info.sample_rate, info.channels, info.bits_per_sample);
    }
    // update when changed to trigger notifications, even though the actual format is fixed
    if (info != AudioDecoder::info) AudioDecoder::setAudioInfo(audioInfoG729);
  }

  bool begin() override {
    TRACEI();
    end();

    input_buffer.resize(G729_ENC_SIZE);
    result_buffer.resize(G729_PCM_SIZE);

    decoder_ctx = initBcg729DecoderChannel();
    if (decoder_ctx == nullptr) {
      LOGE("initBcg729DecoderChannel");
      return false;
    }

    input_pos = 0;
    is_active = true;
    notifyAudioChange(audioInfoG729);
    return true;
  }

  void end() override {
    TRACEI();
    if (decoder_ctx != nullptr) {
      closeBcg729DecoderChannel(decoder_ctx);
      decoder_ctx = nullptr;
    }
    input_pos = 0;
    is_active = false;
  }

  void setOutput(Print& out_stream) override { p_print = &out_stream; }

  operator bool() override { return is_active; }

  size_t write(const uint8_t* data, size_t len) override {
    LOGD("write: %d", static_cast<int>(len));

    if (!is_active) {
      LOGE("inactive");
      return 0;
    }
    if (p_print == nullptr) {
      LOGE("output not set");
      return 0;
    }

    // Fast path: if the caller supplies a complete framed payload and the
    // accumulator is empty, decode directly so SID (2-byte) and speech
    // (10-byte) frame boundaries are preserved.
    if (input_pos == 0 && (len == G729_ENC_SIZE || len == G729_SID_SIZE)) {
      decodeFrame(data, len);
      return len;
    }

    // Stream mode: accumulate bytes into 10-byte speech frames.
    for (size_t j = 0; j < len; ++j) {
      processByte(data[j]);
    }

    return len;
  }

 protected:
  Print* p_print = nullptr;
  bcg729DecoderChannelContextStruct* decoder_ctx = nullptr;
  Vector<uint8_t> input_buffer;
  Vector<uint8_t> result_buffer;
  size_t input_pos = 0;
  bool is_active = false;

  void processByte(uint8_t byte) {
    input_buffer[input_pos++] = byte;

    if (input_pos >= G729_ENC_SIZE) {
      decodeFrame(input_buffer.data(), G729_ENC_SIZE);
      input_pos = 0;
    }
  }

  void decodeFrame(const uint8_t* data, size_t frame_len) {
    if (frame_len != G729_ENC_SIZE && frame_len != G729_SID_SIZE) {
      LOGW("unsupported G729 frame size: %d", static_cast<int>(frame_len));
      return;
    }

    uint8_t sid_frame = frame_len == G729_SID_SIZE ? 1 : 0;

    bcg729Decoder(decoder_ctx, const_cast<uint8_t*>(data),
                  static_cast<uint8_t>(frame_len),
                  0,          // frameErasureFlag
                  sid_frame,  // SIDFrameFlag
                  0,          // rfc3389PayloadFlag
                  reinterpret_cast<int16_t*>(result_buffer.data()));

    p_print->write(result_buffer.data(), result_buffer.size());
  }
};

/**
 * @brief G.729 encoder using the bcg729 library.
 *
 * Accepts 16-bit PCM samples at 8 000 Hz (mono) and writes compressed
 * G.729 frames to the configured output.  Frames are produced every
 * 80 input samples (10 ms):
 *  - Normal speech frame : 10 bytes
 *  - SID / comfort-noise : 2 bytes (only when VAD is enabled)
 *
 * VAD (Voice Activity Detection) is enabled by default; pass `false` to
 * the constructor to disable it and always produce 10-byte speech frames.
 *
 * Input must be exactly 8 000 Hz / mono / 16-bit PCM.  Passing any other
 * AudioInfo via setAudioInfo() logs an error and the fixed format is
 * enforced.
 *
 * Bitrate: 8 kbps (10 bytes × 8 bits/byte ÷ 0.01 s) which gives 1 kbytes/s
 * Compression ratio: 16:1 (w/o SID frame)
 * 
 * @note Requires https://github.com/pschatzmann/codec-bcg729
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class G729Encoder : public AudioEncoder {
 public:
  G729Encoder(bool is_vad_enabled = false) {
    info = audioInfoG729;
    setVADEnabled(is_vad_enabled);
  }

  void setVADEnabled(bool enabled) { vad_enabled = enabled; }

  void setAudioInfo(AudioInfo info) override {
    // Check if requested format is valid
    if (info != audioInfoG729) {
      LOGE(
          "G.729 encoder input is fixed to 8000 Hz, mono, 16-bit PCM "
          "(requested: %d Hz, %d ch, %d bit)",
          info.sample_rate, info.channels, info.bits_per_sample);
    }
    // update when changed to trigger notifications, even though the actual format is fixed
    if (info != AudioEncoder::info) AudioEncoder::setAudioInfo(audioInfoG729);
  }

  bool begin() override {
    TRACEI();
    end();

    encoder_ctx = initBcg729EncoderChannel(vad_enabled ? 1 : 0);
    if (encoder_ctx == nullptr) {
      LOGE("initBcg729EncoderChannel");
      return false;
    }

    input_buffer.resize(G729_PCM_SIZE);
    result_buffer.resize(G729_ENC_SIZE);
    buffer_pos = 0;
    is_active = true;
    return true;
  }

  void end() override {
    TRACEI();
    if (encoder_ctx != nullptr) {
      closeBcg729EncoderChannel(encoder_ctx);
      encoder_ctx = nullptr;
    }
    buffer_pos = 0;
    is_active = false;
  }

  const char* mime() override { return "audio/g729"; }

  void setOutput(Print& out_stream) override { p_print = &out_stream; }

  operator bool() override { return is_active; }

  size_t write(const uint8_t* data, size_t len) override {
    LOGD("write: %d", static_cast<int>(len));

    if (!is_active) {
      LOGE("inactive");
      return 0;
    }
    if (p_print == nullptr) {
      LOGE("output not set");
      return 0;
    }

    for (size_t j = 0; j < len; ++j) {
      processByte(data[j]);
    }
    return len;
  }

 protected:
  Print* p_print = nullptr;
  bcg729EncoderChannelContextStruct* encoder_ctx = nullptr;
  Vector<uint8_t> input_buffer;
  Vector<uint8_t> result_buffer;
  size_t buffer_pos = 0;
  bool vad_enabled = false;
  bool is_active = false;

  void processByte(uint8_t byte) {
    input_buffer[buffer_pos++] = byte;

    if (buffer_pos >= G729_PCM_SIZE) {
      uint8_t result_len = 0;

      bcg729Encoder(encoder_ctx,
                    reinterpret_cast<const int16_t*>(input_buffer.data()),
                    result_buffer.data(), &result_len);

      if (result_len > result_buffer.size()) {
        LOGE("Encoder: result buffer too small: %d -> %d",
             static_cast<int>(result_buffer.size()),
             static_cast<int>(result_len));
      } else if (result_len > 0) {
        p_print->write(result_buffer.data(), result_len);
      }

      buffer_pos = 0;
    }
  }
};

}  // namespace audio_tools