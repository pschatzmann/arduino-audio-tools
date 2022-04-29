/**
 * @file CodecSBC.h
 * @author Phil Schatzmann
 * @brief SBC Codec using  https://github.com/pschatzmann/arduino-libsbc
 * @version 0.1
 * @date 2022-04-24
 */

#pragma once

#include "sbc.h"
#include "sbc/formats.h"
#include "AudioTools/AudioTypes.h"

namespace audio_tools {

/**
 * @brief Decoder for SBC. Depends on
 * https://github.com/pschatzmann/arduino-libsbc.
 * Inspired by sbcdec.c
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
    is_first = true;
    is_active = true;
    sbc_init(&sbc, 0L);
    sbc.endian = SBC_BE;
  }

  virtual void end() {
    sbc_finish(&sbc);
    is_active = false;
  }

  virtual void setNotifyAudioChange(AudioBaseInfoDependent &bi) {
    p_notify = &bi;
  }

  virtual void setOutputStream(Print &out_stream) { p_print = &out_stream; }

  operator boolean() { return is_active; }

  virtual size_t write(const void *data, size_t length) {
    uint8_t *start = (uint8_t *)data;
    int count = length;
    if (is_first) {
      size_t result_len = 0;
      framelen = sbc_decode(&sbc, data, length, result_buffer,
                            result_buffer_size, &result_len);

      // setup input buffer for subsequent decoding stpes
      if (input_buffer != nullptr) delete[] input_buffer;
      input_buffer = new uint8_t[framelen];
      is_first = false;
      start = start + framelen;
      count = length - framelen;

      // audio info
      setup();

      // provide first decoding result
      if (result_len > 0) {
        p_print->write(result_buffer, result_len);
      }
    }

    for (int j = 0; j < count; j++) {
      processByte(start[j]);
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
  void setup() {
    info.bits_per_sample = 16;
    info.channels = sbc.mode == SBC_MODE_MONO ? 1 : 2;
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
    if (p_notify != nullptr) {
      p_notify->setAudioInfo(info);
    }
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
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class SBCEncoder : public AudioEncoder {
 public:
  SBCEncoder(int resultBufferSize = 512, int subbands = 4, int blocks = 4,
             int bitpool = 32, int snr = SBC_AM_LOUDNESS) {
    this->subbands = subbands;
    this->blocks = blocks;
    this->bitpool = bitpool;
    this->snr = snr;
    result_buffer = new uint8_t[resultBufferSize];
  }

  ~SBCEncoder() {
    if (result_buffer != nullptr) delete[] result_buffer;
    if (buffer != nullptr) delete[] buffer;
  }

  void begin() {
    if (sizeof(au_hdr) != 24) {
      /* Sanity check just in case */
      LOGE("FIXME: sizeof(au_hdr) != 24");
      return;
    }
    is_first = true;
    is_active = true;
  }

  virtual void end() {
    sbc_finish(&sbc);
    is_active = false;
  }

  virtual const char *mime() { return "audio/sbc"; }

  virtual void setAudioInfo(AudioBaseInfo info) { this->info = info; }

  virtual void setOutputStream(Print &out_stream) { p_print = &out_stream; }

  operator boolean() { is_active; }

  virtual size_t write(const void *in_ptr, size_t in_size) {
    if (!is_active) {
      return 0;
    }

    const uint8_t *start = (const uint8_t *)in_ptr;
    int size = in_size;

    /// setup from info in header
    if (is_first) {
      is_first = false;
      start = start + sizeof(au_hdr);
      size = in_size - sizeof(au_hdr);

      if (!setup(in_ptr, in_size)) {
        is_active = false;
        return 0;
      }

      int codesize = sbc_get_codesize(&sbc);
      if (codesize != current_codesize) {
        if (buffer != nullptr) delete[] buffer;
        buffer = new uint8_t[codesize];
        current_codesize = codesize;
      }
    }

    // encode bytes
    for (int j = 0; j < size; j++) {
      processByte(start[j]);
    }

    return in_size;
  }

 protected:
  AudioBaseInfo info;
  Print *p_print = nullptr;
  struct au_header au_hdr;
  sbc_t sbc;
  bool is_first = true;
  bool is_active = false;
  int current_codesize = 0;
  uint8_t *buffer = nullptr;
  int buffer_pos = 0;
  uint8_t *result_buffer = nullptr;
  int subbands = 4;
  int blocks = 4;
  int bitpool = 32;
  int snr;

  /// Determines audio information and calls sbc_init;
  bool setup(const void *in_ptr, size_t len) {
    if (len < (ssize_t)sizeof(au_hdr)) {
      return false;
    }

    memmove(&au_hdr, in_ptr, sizeof(au_hdr));
    if (au_hdr.magic != AU_MAGIC || BE_INT(au_hdr.hdr_size) > 128 ||
        BE_INT(au_hdr.hdr_size) < sizeof(au_hdr) ||
        BE_INT(au_hdr.encoding) != AU_FMT_LIN16) {
      LOGE("Not in Sun/NeXT audio S16_BE format");
      return false;
    }

    sbc_init(&sbc, 0L);

    switch (BE_INT(info.sample_rate)) {
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
        LOGE("Invalid sample_rate")
        return false;
    }

    switch (BE_INT(info.channels)) {
      case 1:
        sbc.mode = SBC_MODE_MONO;
        break;
      case 2:
        sbc.mode = SBC_MODE_STEREO;
        break;
      default:
        LOGE("Invalid channels")
        return false;
    }

    sbc.subbands = subbands == 4 ? SBC_SB_4 : SBC_SB_8;
    sbc.endian = SBC_BE;

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
      sbc_encode(&sbc, buffer, current_codesize, result_buffer, 512, &written);
      if (written > 0) {
        p_print->write(result_buffer, written);
      }
      buffer_pos = 0;
    }
  }
};

}  // namespace audio_tools