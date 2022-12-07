/**
 * @file CodecCodec2.h
 * @author Phil Schatzmann
 * @brief Codec2 Codec using  https://github.com/pschatzmann/arduino-codec2
 * The codec was developed by David Grant Rowe, with support and cooperation of other researchers 
 * (e.g., Jean-Marc Valin from Opus).
 * Codec 2 consists of 3200, 2400, 1600, 1400, 1300, 1200, 700 and 450 bit/s codec modes. 
 * It outperforms most other low-bitrate speech codecs. For example, it uses half the bandwidth 
 * of Advanced Multi-Band Excitation to encode speech with similar quality.
 * The speech codec uses 16-bit PCM sampled audio, and outputs packed digital bytes. 
 * When sent packed digital bytes, it outputs PCM sampled audio. The audio sample rate is fixed 
 * at 8 kHz.
 *
 * @version 0.1
 * @date 2022-04-24
 */

#pragma once

#include "AudioCodecs/AudioEncoded.h"
#include "codec2.h"

/** 
 * @defgroup codec2 Codec2
 * @ingroup codecs
 * @brief Codec2   
**/

namespace audio_tools {

/// Convert bits per sample to Codec2 mode
int getCodec2Mode(int bits_per_second) {
  switch (bits_per_second) {
    case 3200:
      return CODEC2_MODE_3200;
    case 2400:
      return CODEC2_MODE_2400;
    case 1600:
      return CODEC2_MODE_1600;
    case 1400:
      return CODEC2_MODE_1400;
    case 1300:
      return CODEC2_MODE_1300;
    case 1200:
      return CODEC2_MODE_1200;
    case 700:
      return CODEC2_MODE_700C;
    case 450:
      return CODEC2_MODE_450;
    default:
      LOGE("Unsupported sample rate: use 3200, 2400, 1600, 1400, 1300, 1200, 700 or 450");
      return -1;
  }
}

/**
 * @brief Decoder for Codec2. Depends on
 * https://github.com/pschatzmann/arduino-libcodec2.
 * @ingroup codec2
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class Codec2Decoder : public AudioDecoder {
public:
  Codec2Decoder() {
    cfg.sample_rate = 8000;
    cfg.channels = 1;
    cfg.bits_per_sample = 16;
  }
  /// sets bits per second: 3200, 2400, 1600, 1400, 1300, 1200, 700 and 450 bit/s
  virtual void setBitsPerSecond(int bps){
    bits_per_second = bps;
  }

  int bitsPerSecond() {
    return bits_per_second;
  }

  virtual void setAudioInfo(AudioBaseInfo cfg) { this->cfg = cfg; }

  virtual AudioBaseInfo audioInfo() { return cfg; }

  virtual void begin(AudioBaseInfo cfg) {
    setAudioInfo(cfg);
    begin();
  }

  virtual void begin() {
    TRACEI();

    int mode = getCodec2Mode(bits_per_second);
    if (mode==-1){
      return;
    }
    if (cfg.channels!=1){
      LOGE("Only 1 channel supported")
      return;
    }
    if (cfg.bits_per_sample!=16){
      LOGE("Only 16 bps are supported")
      return;
    }
    if (cfg.sample_rate!=8000){
      LOGW("Sample rate should be 8000: %d", cfg.sample_rate);
    }

    p_codec2 = codec2_create(mode);
    if (p_codec2==nullptr){
      LOGE("codec2_create");
      return;
    }

    result_buffer.resize(codec2_samples_per_frame(p_codec2));
    input_buffer.resize(codec2_bytes_per_frame(p_codec2));

    if (p_notify != nullptr) {
      p_notify->setAudioInfo(cfg);
    }
    is_active = true;
  }

  virtual void end() {
    TRACEI();
    codec2_destroy(p_codec2);
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

    uint8_t *p_byte = (uint8_t *)data;
    for (int j = 0; j < length; j++) {
      processByte(p_byte[j]);
    }

    return length;
  }

protected:
  Print *p_print = nullptr;
  struct CODEC2 *p_codec2;
  AudioBaseInfo cfg;
  AudioBaseInfoDependent *p_notify = nullptr;
  bool is_active = false;
  Vector<uint8_t> input_buffer;
  Vector<int16_t> result_buffer;
  int input_pos = 0;
  int bits_per_second=2400;

  /// Build decoding buffer and decode when frame is full
  void processByte(uint8_t byte) {
    // add byte to buffer
    input_buffer[input_pos++] = byte;

    // decode if buffer is full
    if (input_pos >= input_buffer.size()) {
      codec2_decode(p_codec2, result_buffer.data(), input_buffer.data());
      p_print->write((uint8_t*)result_buffer.data(), result_buffer.size()*2);
      input_pos = 0;
    }
  }
};

/**
 * @brief Encoder for Codec2 - Depends on
 * https://github.com/pschatzmann/arduino-libcodec2.
 * @ingroup codec2
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class Codec2Encoder : public AudioEncoder {
public:
  Codec2Encoder() {
    cfg.sample_rate = 8000;
    cfg.channels = 1;
    cfg.bits_per_sample = 16;
  }

  /// sets bits per second: 3200, 2400, 1600, 1400, 1300, 1200, 700 and 450 bit/s
  virtual void setBitsPerSecond(int bps){
    bits_per_second = bps;
  }

  int bitsPerSecond() {
    return bits_per_second;
  }

  void begin(AudioBaseInfo bi) {
    setAudioInfo(bi);
    begin();
  }

  void begin() {
    TRACEI();

    int mode = getCodec2Mode(bits_per_second);
    if (mode==-1){
      return;
    }
    if (cfg.channels!=1){
      LOGE("Only 1 channel supported")
      return;
    }
    if (cfg.bits_per_sample!=16){
      LOGE("Only 16 bps are supported")
      return;
    }
    if (cfg.sample_rate!=8000){
      LOGW("Sample rate should be 8000: %d", cfg.sample_rate);
    }

    p_codec2 = codec2_create(mode);
    if (p_codec2==nullptr){
      LOGE("codec2_create");
      return;
    }

    input_buffer.resize(codec2_samples_per_frame(p_codec2)*sizeof(int16_t));
    result_buffer.resize(codec2_bytes_per_frame(p_codec2));
    is_active = true;
  }

  virtual void end() {
    TRACEI();
    codec2_destroy(p_codec2);
    is_active = false;
  }

  virtual const char *mime() { return "audio/codec2"; }

  virtual void setAudioInfo(AudioBaseInfo cfg) { this->cfg = cfg; }

  virtual void setOutputStream(Print &out_stream) { p_print = &out_stream; }

  operator bool() { return is_active; }

  virtual size_t write(const void *in_ptr, size_t in_size) {
    LOGD("write: %d", in_size);
    if (!is_active) {
      LOGE("inactive");
      return 0;
    }
    // encode bytes
    uint8_t *p_byte = (uint8_t *)in_ptr;
    for (int j = 0; j < in_size; j++) {
      processByte(p_byte[j]);
    }
    return in_size;
  }

protected:
  AudioBaseInfo cfg;
  Print *p_print = nullptr;
  struct CODEC2 *p_codec2;
  bool is_active = false;
  int buffer_pos = 0;
  Vector<uint8_t> input_buffer;
  Vector<uint8_t> result_buffer;
  int bits_per_second=2400;

  // add byte to decoding buffer and decode if buffer is full
  void processByte(uint8_t byte) {
    input_buffer[buffer_pos++] = byte;
    if (buffer_pos >= input_buffer.size()) {
      // encode
      codec2_encode(p_codec2, result_buffer.data(),(int16_t*)input_buffer.data());
      p_print->write(result_buffer.data(), result_buffer.size());
      buffer_pos = 0;
    }
  }
};

} // namespace audio_tools