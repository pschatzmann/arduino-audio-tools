/**
 * @file CodecSBC.h
 * @author Phil Schatzmann
 * @brief SBC Codec using  https://github.com/pschatzmann/arduino-libsbc
 * @version 0.1
 * @date 2022-04-24
 */

#pragma once

#include "AudioCodecs/AudioEncoded.h"
#include "sbc.h"
#include "sbc/formats.h"

/** 
 * @defgroup codec-sbc SBC
 * @ingroup codecs
 * @brief Codec SBC   
**/

namespace audio_tools {

/**
 * @brief Decoder for SBC. Depends on
 * https://github.com/pschatzmann/arduino-libsbc.
 * Inspired by sbcdec.c
 * @ingroup codec-sbc
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class SBCDecoder : public AudioDecoder {
 public:
  SBCDecoder(int bufferSize = 8192) {
    result_buffer = new uint8_t[bufferSize];
    result_buffer_size = bufferSize;
  }

  ~SBCDecoder() {
    if (result_buffer != nullptr) delete[] result_buffer;
    if (input_buffer != nullptr) delete[] input_buffer;
  }

  virtual AudioBaseInfo audioInfo() { return info; }

  virtual void begin() {
    TRACEI();
    is_first = true;
    is_active = true;
    sbc_init(&sbc, 0L);
  }

  virtual void end() {
    TRACEI();
    sbc_finish(&sbc);
    is_active = false;
  }

  virtual void setNotifyAudioChange(AudioBaseInfoDependent &bi) {
    p_notify = &bi;
  }

  virtual void setOutputStream(Print &out_stream) { p_print = &out_stream; }

  operator bool() { return is_active; }

  virtual size_t write(const void *data, size_t length) {
    LOGD("write: %d", length);
    if (!is_active) {
      LOGE("inactive");
      return 0;
    }

    uint8_t *start = (uint8_t *)data;
    int count = length;
    if (is_first) {
      framelen = firstWrite(data, length);
      LOGI("framelen: %d", framelen);
      // check if we have a valid frame length
      if (isValidFrameLen(framelen)) {
        start = start + framelen;
        count = length - framelen;
        is_first = false;
      }
    }

    if (!is_first){
      for (int j = 0; j < count; j++) {
        processByte(start[j]);
      }
    }

    return length;
  }

 protected:
  Print *p_print = nullptr;
  AudioBaseInfo info;
  AudioBaseInfoDependent *p_notify = nullptr;
  sbc_t sbc;
  bool is_first = true;
  bool is_active = false;
  uint8_t *result_buffer = nullptr;
  int result_buffer_size;
  int framelen;
  uint8_t *input_buffer = nullptr;
  int input_pos = 0;

  /// Process audio info
  void setupAudioInfo() {
    info.bits_per_sample = 16;
    info.channels = sbc.mode == SBC_MODE_MONO ? 1 : 2;
    LOGI("channels: %d", info.channels);
    switch (sbc.frequency) {
      case SBC_FREQ_16000:
        info.sample_rate = 16000;
        break;
      case SBC_FREQ_32000:
        info.sample_rate = 32000;
        break;
      case SBC_FREQ_44100:
        info.sample_rate = 44100;
        break;
      case SBC_FREQ_48000:
        info.sample_rate = 48000;
        break;
      default:
        LOGE("Unsupported sample rate");
        info.sample_rate = 0;
        break;
    }
    LOGI("sample_rate: %d", info.sample_rate);
    if (p_notify != nullptr) {
      p_notify->setAudioInfo(info);
    }
  }

  bool isValidFrameLen(int len){
    return len > 0 && len < 256;
  }

  /// Determines the framelen
  int firstWrite(const void *data, size_t length) {
    size_t result_len = 0;
    int frame_len = sbc_parse(&sbc, data, length);
    if (isValidFrameLen(frame_len)){

      // setup audio info
      setupAudioInfo();

      // setup input buffer for subsequent decoding stpes
      setupInputBuffer(frame_len);
    }

    return frame_len;
  }

  void setupInputBuffer(int len) {
    LOGI("input_buffer: %d", len);
    if (input_buffer != nullptr) delete[] input_buffer;
    input_buffer = new uint8_t[len];
  }

  /// Build decoding buffer and decode when frame is full
  void processByte(uint8_t byte) {
    // add byte to buffer
    input_buffer[input_pos++] = byte;

    // decode if buffer is full
    if (input_pos >= framelen) {
      size_t result_len = 0;
      sbc_decode(&sbc, input_buffer, framelen, result_buffer,
                 result_buffer_size, &result_len);
      if (result_len > 0) {
        p_print->write(result_buffer, result_len);
      }
      input_pos = 0;
    }
  }
};

/**
 * @brief Encoder for SBC - Depends on
 * https://github.com/pschatzmann/arduino-libsbc.
 * Inspired by sbcenc.c
 * @ingroup codec-sbc
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class SBCEncoder : public AudioEncoder {
 public:
  SBCEncoder(int resultBufferSize = 1024, int subbands = SBC_SB_8,
             int blocks = 16, int bitpool = 32,
             int snr = SBC_AM_LOUDNESS) {
    this->subbands = subbands;
    this->blocks = blocks;
    this->bitpool = bitpool;
    this->snr = snr;
    this->result_buffer_size = resultBufferSize;
    result_buffer = new uint8_t[resultBufferSize];
  }

  ~SBCEncoder() {
    if (result_buffer != nullptr) delete[] result_buffer;
    if (buffer != nullptr) delete[] buffer;
  }

  void begin(AudioBaseInfo bi) {
    setAudioInfo(bi);
    begin();
  }

  void begin() {
    TRACEI();
    is_first = true;
    is_active = setup();
    int codesize = sbc_get_codesize(&sbc);
    if (codesize != current_codesize) {
      if (buffer != nullptr) delete[] buffer;
      buffer = new uint8_t[codesize];
      current_codesize = codesize;
    }
  }

  virtual void end() {
    TRACEI();
    sbc_finish(&sbc);
    is_active = false;
  }

  virtual const char *mime() { return "audio/sbc"; }

  virtual void setAudioInfo(AudioBaseInfo info) { this->info = info; }

  virtual void setOutputStream(Print &out_stream) { p_print = &out_stream; }

  operator bool() { return is_active; }

  virtual size_t write(const void *in_ptr, size_t in_size) {
    LOGD("write: %d", in_size);
    if (!is_active) {
      LOGE("inactive");
      return 0;
    }
    const uint8_t *start = (const uint8_t *)in_ptr;

    // encode bytes
    for (int j = 0; j < in_size; j++) {
      processByte(start[j]);
    }

    return in_size;
  }

 protected:
  AudioBaseInfo info;
  Print *p_print = nullptr;
  sbc_t sbc;
  bool is_first = true;
  bool is_active = false;
  int current_codesize = 0;
  uint8_t *buffer = nullptr;
  int buffer_pos = 0;
  uint8_t *result_buffer = nullptr;
  int result_buffer_size = 0;
  int result_size = 0;
  int subbands = 4;
  int blocks = 4;
  int bitpool = 32;
  int snr;

  /// Determines audio information and calls sbc_init;
  bool setup() {
    sbc_init(&sbc, 0L);

    switch (info.sample_rate) {
      case 16000:
        sbc.frequency = SBC_FREQ_16000;
        break;
      case 32000:
        sbc.frequency = SBC_FREQ_32000;
        break;
      case 44100:
        sbc.frequency = SBC_FREQ_44100;
        break;
      case 48000:
        sbc.frequency = SBC_FREQ_48000;
        break;
      default:
        LOGE("Invalid sample_rate: %d", info.sample_rate);
        return false;
    }

    switch (info.channels) {
      case 1:
        sbc.mode = SBC_MODE_MONO;
        break;
      case 2:
        sbc.mode = SBC_MODE_STEREO;
        break;
      default:
        LOGE("Invalid channels: %d", info.channels);
        return false;
    }

    sbc.subbands = subbands == 4 ? SBC_SB_4 : SBC_SB_8;
    // sbc.endian = SBC_LE;

    sbc.bitpool = bitpool;
    sbc.allocation = snr ? SBC_AM_SNR : SBC_AM_LOUDNESS;
    sbc.blocks = blocks == 4    ? SBC_BLK_4
                 : blocks == 8  ? SBC_BLK_8
                 : blocks == 12 ? SBC_BLK_12
                                : SBC_BLK_16;

    return true;
  }

  // add byte to decoding buffer and decode if buffer is full
  void processByte(uint8_t byte) {
    buffer[buffer_pos++] = byte;
    if (buffer_pos >= current_codesize) {
      ssize_t written;
      // Encodes ONE input block into ONE output block */
      // ssize_t sbc_encode(sbc_t *sbc, const void *input, size_t input_len,
      // void *output, size_t output_len, ssize_t *written);
      sbc_encode(&sbc, buffer, current_codesize, result_buffer + result_size,
                 result_buffer_size - result_size, &written);
      result_size += written;
      if (result_size + written >= result_buffer_size) {
        LOGI("result_size: %d (%d)", result_size, written);
        p_print->write(result_buffer, result_size);
        result_size = 0;
      }
      buffer_pos = 0;
    }
  }
};

}  // namespace audio_tools