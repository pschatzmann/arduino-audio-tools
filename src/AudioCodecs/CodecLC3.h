/**
 * @file CodecLC3.h
 * @author Phil Schatzmann
 * @brief Codec for lc3 using https://github.com/pschatzmann/arduino-liblc3
 * @version 0.1
 * @date 2022-04-24
 *
 * @copyright Copyright (c) 2022
 *
 */
#pragma once

#include "AudioCodecs/AudioCodecsBase.h"
#include "lc3.h"

namespace audio_tools {

// 20 to 400
#define DEFAULT_BYTE_COUNT 40
// 7500 or 10000
#define LC3_DEFAULT_DT_US 7500

/**
 * @brief Decoder for LC3. Depends on
 * https://github.com/pschatzmann/arduino-liblc3
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class LC3Decoder : public AudioDecoder {
 public:
  LC3Decoder(AudioInfo info, int dt_us = LC3_DEFAULT_DT_US,
             uint16_t inputByteCount = DEFAULT_BYTE_COUNT) {
    this->dt_us = dt_us;
    this->info = info;
    this->input_byte_count = inputByteCount;
  }

  LC3Decoder(int dt_us = LC3_DEFAULT_DT_US,
             uint16_t inputByteCount = DEFAULT_BYTE_COUNT) {
    this->dt_us = dt_us;
    this->input_byte_count = inputByteCount;
    info.sample_rate = 32000;
    info.bits_per_sample = 16;
    info.channels = 1;
  }

  virtual bool begin() {
    TRACEI();

    // Return the number of PCM samples in a frame
    num_frames = lc3_frame_samples(dt_us, info.sample_rate);
    dec_size = lc3_decoder_size(dt_us, info.sample_rate);

    LOGI("channels: %d", info.channels);
    LOGI("sample_rate: %d", info.sample_rate);
    LOGI("input_byte_count: %d", input_byte_count);
    LOGI("dt_us: %d", dt_us);
    LOGI("num_frames: %d", num_frames);
    LOGI("dec_size: %d", dec_size);

    if (!checkValues()) {
      LOGE("Invalid Parameters");
      return false;
    }

    // setup memory
    input_buffer.resize(input_byte_count);
    output_buffer.resize(num_frames * 2);
    lc3_decoder_memory.resize(dec_size);

    // setup decoder
    lc3_decoder = lc3_setup_decoder(dt_us, info.sample_rate, 0,
                                    (void *)lc3_decoder_memory.data());
    notifyAudioChange(info);

    input_pos = 0;
    active = true;
    return true;
  }

  virtual void end() {
    TRACEI();
    active = false;
  }

  virtual void setOutput(Print &out_stream) { p_print = &out_stream; }

  operator bool() { return active; }

  virtual size_t write(const uint8_t *data, size_t len) {
    if (!active) return 0;
    LOGD("write %u", len);

    uint8_t *p_ptr8 = (uint8_t *)data;

    for (int j = 0; j < len; j++) {
      input_buffer[input_pos++] = p_ptr8[j];
      if (input_pos >= input_buffer.size()) {
        if (lc3_decode(lc3_decoder, input_buffer.data(), input_buffer.size(),
                       pcm_format, (int16_t *)output_buffer.data(), 1) != 0) {
          LOGE("lc3_decode");
        }

        // write all data to final output
        int requested = output_buffer.size();
        int written =
            p_print->write((const uint8_t *)output_buffer.data(), requested);
        if (written != requested) {
          LOGE("Decoder Bytes requested: %d - written: %d", requested, written);
        }
        input_pos = 0;
      }
    }
    return len;
  }

 protected:
  Print *p_print = nullptr;
  lc3_decoder_t lc3_decoder = nullptr;
  lc3_pcm_format pcm_format;
  Vector<uint8_t> lc3_decoder_memory;
  Vector<uint16_t> output_buffer;
  Vector<uint8_t> input_buffer;
  size_t input_pos = 0;
  int dt_us;
  uint16_t input_byte_count = 20;  // up to 400
  uint16_t num_frames;
  unsigned dec_size;
  bool active = false;

  bool checkValues() {
    if (p_print == nullptr) {
      LOGE("Output is not defined");
      return false;
    }

    if (!LC3_CHECK_DT_US(dt_us)) {
      LOGE("dt_us: %d", dt_us);
      return false;
    }

    if (!LC3_CHECK_SR_HZ(info.sample_rate)) {
      LOGE("sample_rate: %d", info.sample_rate);
      return false;
    }

    if (info.channels!=1){
      LOGE("channels: %d", info.channels);
    }

    if (num_frames == -1) {
      LOGE("num_frames could not be determined - using m");
      return false;
    }

    if (dec_size == 0) {
      LOGE("dec_size");
      return false;
    }

    switch (info.bits_per_sample) {
      case 16:
        pcm_format = LC3_PCM_FORMAT_S16;
        break;
      case 24:
        pcm_format = LC3_PCM_FORMAT_S24;
        break;
      default:
        LOGE("Bits per sample not supported: %d", info.bits_per_sample);
        return false;
    }
    return true;
  }
};

/**
 * @brief Encoder for LC3 - Depends on
 * https://github.com/pschatzmann/arduino-liblc3
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class LC3Encoder : public AudioEncoder {
 public:
  LC3Encoder(int dt_us = LC3_DEFAULT_DT_US,
             uint16_t outputByteCount = DEFAULT_BYTE_COUNT) {
    this->dt_us = dt_us;
    info.sample_rate = 32000;
    info.bits_per_sample = 16;
    info.channels = 1;
    output_byte_count = outputByteCount;
  }

  bool begin() {
    TRACEI();

    unsigned enc_size = lc3_encoder_size(dt_us, info.sample_rate);
    num_frames = lc3_frame_samples(dt_us, info.sample_rate);

    LOGI("sample_rate: %d", info.sample_rate);
    LOGI("channels: %d", info.channels);
    LOGI("dt_us: %d", dt_us);
    LOGI("output_byte_count: %d", output_byte_count);
    LOGI("enc_size: %d", enc_size);
    LOGI("num_frames: %d", num_frames);

    if (!checkValues()) {
      return false;
    }

    // setup memory
    lc3_encoder_memory.resize(enc_size);
    input_buffer.resize(num_frames * 2);
    output_buffer.resize(output_byte_count);

    // setup encoder
    lc3_encoder = lc3_setup_encoder(dt_us, info.sample_rate, 0,
                                    lc3_encoder_memory.data());

    input_pos = 0;
    active = true;
    return true;
  }

  virtual void end() {
    TRACEI();
    active = false;
  }

  virtual const char *mime() { return "audio/lc3"; }

  virtual void setOutput(Print &out_stream) { p_print = &out_stream; }

  operator bool() { return lc3_encoder != nullptr; }

  virtual size_t write(const uint8_t *data, size_t len) {
    if (!active) return 0;
    LOGD("write %u", len);
    uint8_t *p_ptr8 = (uint8_t *)data;

    for (int j = 0; j < len; j++) {
      input_buffer[input_pos++] = p_ptr8[j];
      if (input_pos >= num_frames * 2) {
        if (lc3_encode(lc3_encoder, pcm_format,
                       (const int16_t *)input_buffer.data(), 1,
                       output_buffer.size(), output_buffer.data()) != 0) {
          LOGE("lc3_encode");
        }

        // write all data to final output
        int requested = output_buffer.size();
        int written = p_print->write(output_buffer.data(), requested);
        if (written != requested) {
          LOGE("Encoder Bytes requested: %d - written: %d", requested, written);
        }
        input_pos = 0;
      }
    }
    return len;
  }

 protected:
  Print *p_print = nullptr;
  unsigned dt_us = 1000;
  uint16_t num_frames;
  lc3_encoder_t lc3_encoder = nullptr;
  lc3_pcm_format pcm_format;
  uint16_t output_byte_count = 20;
  Vector<uint8_t> lc3_encoder_memory;
  Vector<uint8_t> output_buffer;
  Vector<uint8_t> input_buffer;
  int input_pos = 0;
  bool active = false;

  bool checkValues() {
    if (p_print == nullptr) {
      LOGE("Output is not defined");
      return false;
    }

    if (!LC3_CHECK_DT_US(dt_us)) {
      LOGE("dt_us: %d", dt_us);
      return false;
    }

    if (!LC3_CHECK_SR_HZ(info.sample_rate)) {
      LOGE("sample_rate: %d", info.sample_rate);
      return false;
    }

    if (info.channels!=1){
      LOGE("channels: %d", info.channels);
    }

    if (num_frames == -1) {
      LOGE("Invalid num_frames");
      return false;
    }

    switch (info.bits_per_sample) {
      case 16:
        pcm_format = LC3_PCM_FORMAT_S16;
        break;
      case 24:
        pcm_format = LC3_PCM_FORMAT_S24;
        break;
      default:
        LOGE("Bits per sample not supported: %d", info.bits_per_sample);
        return false;
    }
    return true;
  }
};

}  // namespace audio_tools