/**
 * @file CodecG.722.h
 * @author Phil Schatzmann
 * @brief G.722 Codec using  https://github.com/pschatzmann/arduino-libg722
 * @version 0.1
 * @date 2022-04-24
 */

#pragma once

#include "AudioCodecs/AudioCodecsBase.h"
#include "g722_codec.h"

// size in bytes
#define G722_PCM_SIZE 80
#define G722_ENC_SIZE 40


namespace audio_tools {

/**
 * @brief Decoder for G.722. Depends on
 * https://github.com/pschatzmann/arduino-libg722.
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class G722Decoder : public AudioDecoder {
 public:
  G722Decoder() = default;

  /// Defines the options for the G.722 Codec: G722_SAMPLE_RATE_8000,G722_PACKED
  void setOptions(int options){
    this->options = options;
  }

  virtual bool begin() {
    TRACEI();
    input_buffer.resize(10);
    result_buffer.resize(40);

    g722_dctx = g722_decoder_new(info.sample_rate, options);
    if (g722_dctx == nullptr) {
      LOGE("g722_decoder_new");
      return false;
    }

    notifyAudioChange(info);
    is_active = true;
    return true;
  }

  virtual void end() {
    TRACEI();
    g722_decoder_destroy(g722_dctx);
    is_active = false;
  }

  virtual void setOutput(Print &out_stream) { p_print = &out_stream; }

  operator bool() { return is_active; }

  virtual size_t write(const uint8_t *data, size_t len) {
    LOGD("write: %d", len);
    if (!is_active) {
      LOGE("inactive");
      return 0;
    }

    uint8_t *p_byte = (uint8_t *)data;
    for (int j = 0; j < len; j++) {
      processByte(p_byte[j]);
    }

    return len;
  }

 protected:
  Print *p_print = nullptr;
  G722_DEC_CTX *g722_dctx=nullptr;
  Vector<uint8_t> input_buffer;
  Vector<uint8_t> result_buffer;
  int options = G722_SAMPLE_RATE_8000;
  int input_pos = 0;
  bool is_active = false;

  /// Build decoding buffer and decode when frame is full
  void processByte(uint8_t byte) {
    // add byte to buffer
    input_buffer[input_pos++] = byte;

    // decode if buffer is full
    if (input_pos >= input_buffer.size()) {
      int result_samples = g722_decode(g722_dctx, input_buffer.data(), input_buffer.size(),
                  (int16_t *)result_buffer.data());

      if (result_samples*2>result_buffer.size()){
        LOGE("Decoder:Result buffer too small: %d -> %d",result_buffer.size(),result_samples*2);
      }

      p_print->write(result_buffer.data(), result_samples);
      input_pos = 0;
    }
  }
};

/**
 * @brief Encoder for G.722 - Depends on
 * https://github.com/pschatzmann/arduino-libg722.
 * Inspired by g722enc.c
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class G722Encoder : public AudioEncoder {
 public:
  G722Encoder() = default;

  /// Defines the options for the G.722 Codec: G722_SAMPLE_RATE_8000,G722_PACKED
  void setOptions(int options){
    this->options = options;
  }

  bool begin() {
    TRACEI();
    if (info.channels != 1) {
      LOGW("1 channel expected, was: %d", info.channels);
    }

    g722_ectx = g722_encoder_new(info.sample_rate, options);
    if (g722_ectx == NULL) {
      LOGE("g722_encoder_new");
      return false;
    }

    input_buffer.resize(G722_PCM_SIZE);
    result_buffer.resize(G722_ENC_SIZE);
    is_active = true;
    return true;
  }

  virtual void end() {
    TRACEI();
    g722_encoder_destroy(g722_ectx);
    is_active = false;
  }

  virtual const char *mime() { return "audio/g722"; }

  virtual void setOutput(Print &out_stream) { p_print = &out_stream; }

  operator bool() { return is_active; }

  virtual size_t write(const uint8_t *data, size_t len) {
    LOGD("write: %d", len);
    if (!is_active) {
      LOGE("inactive");
      return 0;
    }
    // encode bytes
    uint8_t *p_byte = (uint8_t *)data;
    for (int j = 0; j < len; j++) {
      processByte(p_byte[j]);
    }
    return len;
  }

 protected:
  Print *p_print = nullptr;
  G722_ENC_CTX *g722_ectx = nullptr;
  Vector<uint8_t> input_buffer;
  Vector<uint8_t> result_buffer;
  int options = G722_SAMPLE_RATE_8000;
  int buffer_pos = 0;
  bool is_active = false;

  // add byte to decoding buffer and decode if buffer is full
  void processByte(uint8_t byte) {
    input_buffer[buffer_pos++] = byte;
    if (buffer_pos >= input_buffer.size()) {
      // convert for little endian
      int samples = input_buffer.size() / 2;
      // encode
     int result_len = g722_encode(g722_ectx,(const int16_t*) input_buffer.data(), samples,
                  result_buffer.data());
      if (result_len>result_buffer.size()){
        LOGE("Encoder:Result buffer too small: %d -> %d",result_buffer.size(),result_len);
      }
      p_print->write(result_buffer.data(), result_len);
      buffer_pos = 0;
    }
  }
};

}  // namespace audio_tools