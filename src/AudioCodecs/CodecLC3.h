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

#include "AudioTools/AudioTypes.h"
#include "lc3.h"

namespace audio_tools {

/**
 * @brief Decoder for LC3. Depends on
 * https://github.com/pschatzmann/arduino-liblc3
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class LC3Decoder : public AudioDecoder {
 public:
  LC3Decoder(AudioBaseInfo info, int dt_us = 1000,
             uint16_t inputByteCount = 20) {
    this->dt_us = dt_us;
    this->info = info;
    this->input_byte_count = inputByteCount;
  }

  LC3Decoder(int dt_us = 1000,
             uint16_t inputByteCount = 20) {
    this->dt_us = dt_us;
    this->input_byte_count = inputByteCount;
    info.sample_rate = 44100;
    info.bits_per_sample = 16;
    info.channels = 2;
  }

  virtual AudioBaseInfo audioInfo() { return info; }

  virtual void begin() {
    if (p_print == nullptr) {
      LOGE("Output is not defined");
      return;
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
        return;
    }

    unsigned dec_size = lc3_decoder_size(dt_us, info.sample_rate);
    lc3_decoder_mem.resize(dec_size);
    lc3_decoder =
        lc3_setup_decoder(dt_us, info.sample_rate, 0, (void*) lc3_decoder_mem.data());
    num_frames = lc3_frame_samples(dt_us, info.sample_rate);
    output_buffer.resize(num_frames);
    input_buffer.resize(input_byte_count);
    input_pos = 0;

    if (p_notify != nullptr) {
      p_notify->setAudioInfo(info);
    }
  }

  virtual void end() {
  }

  virtual void setNotifyAudioChange(AudioBaseInfoDependent &bi) {
    p_notify = &bi;
  }

  virtual void setOutputStream(Print &out_stream) { p_print = &out_stream; }

  operator boolean() { return lc3_decoder_mem.size() > 0; }

  virtual size_t write(const void *input, size_t length) {
    uint8_t *p_ptr8 = (uint8_t *)input;

    for (int j = 0; j < length; j++) {
      input_buffer[input_pos++] = p_ptr8[j];
      if (input_pos >= input_buffer.size()) {
        lc3_decode(lc3_decoder, input_buffer.data(), input_buffer.size(), pcm_format,
                   (int16_t *)output_buffer.data(), 1);
        p_print->write((const uint8_t *)output_buffer.data(), output_buffer.size());
        input_pos = 0;
      }
    }
    return length;
  }

 protected:
  Print *p_print = nullptr;
  AudioBaseInfo info;
  AudioBaseInfoDependent *p_notify = nullptr;
  lc3_decoder_t lc3_decoder = nullptr;
  lc3_pcm_format pcm_format;
  Vector<uint8_t> lc3_decoder_mem;
  Vector<uint16_t> output_buffer;
  Vector<uint8_t> input_buffer;
  size_t input_pos = 0;
  int dt_us;
  uint16_t input_byte_count = 20;  // up to 400
  uint16_t num_frames;
};

/**
 * @brief Encoder for LC3 - Depends on
 * https://github.com/pschatzmann/arduino-liblc3
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class LC3Encoder : public AudioEncoder {
 public:
  LC3Encoder(int dt_us = 1000, uint16_t outputByteCount = 20) {
    this->dt_us = dt_us;
    output_byte_count = outputByteCount;
  }

  void begin() {
    if (p_print == nullptr) {
      LOGE("Output is not defined");
      return;
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
        return;
    }

    unsigned enc_size = lc3_encoder_size(dt_us, info.sample_rate);
    lc3_encoder_mem.resize(enc_size);
    num_frames = lc3_frame_samples(dt_us, info.sample_rate);
    lc3_encoder =
        lc3_setup_encoder(dt_us, info.sample_rate, 0, lc3_encoder_mem.data());
    input.resize(num_frames * 2);
    output.resize(output_byte_count);
    input_pos = 0;
  }

  virtual void end() {}

  virtual const char *mime() { return "audio/lc3"; }

  virtual void setAudioInfo(AudioBaseInfo info) { this->info = info; }

  virtual void setOutputStream(Print &out_stream) { p_print = &out_stream; }

  operator boolean() { lc3_encoder != nullptr; }

  virtual size_t write(const void *in_ptr, size_t in_size) {
    uint8_t *p_ptr8 = (uint8_t *) in_ptr;
    for (int j = 0; j < in_size; j++) {
      input[input_pos++] = p_ptr8[j];
      if (input_pos >= num_frames * 2) {
        lc3_encode(lc3_encoder, pcm_format, (const int16_t *)input.data(), 1,
                   output.size(), output.data());
        p_print->write(output.data(), output.size());
        input_pos = 0;
      }
    }
    return in_size;
  }

 protected:
  AudioBaseInfo info;
  Print *p_print = nullptr;
  unsigned dt_us = 1000;
  uint16_t num_frames;
  lc3_encoder_t lc3_encoder = nullptr;
  lc3_pcm_format pcm_format;
  uint16_t output_byte_count = 20;
  Vector<uint8_t> lc3_encoder_mem;
  Vector<uint8_t> output;
  Vector<uint8_t> input;
  int input_pos = 0;
};

}  // namespace audio_tools