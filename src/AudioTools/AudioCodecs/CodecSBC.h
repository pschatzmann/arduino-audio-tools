/**
 * @file CodecSBC.h
 * @author Phil Schatzmann
 * @brief SBC Codec using  https://github.com/pschatzmann/arduino-libsbc
 * @version 0.1
 * @date 2022-04-24
 */

#pragma once

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "sbc.h"
#include "sbc/formats.h"


namespace audio_tools {

/**
 * @brief Decoder for SBC. Depends on
 * https://github.com/pschatzmann/arduino-libsbc.
 * Inspired by sbcdec.c
 * @ingroup codecs
 * @ingroup decoder
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
    if (result_buffer != nullptr)
      delete[] result_buffer;
    if (input_buffer != nullptr)
      delete[] input_buffer;
  }

  virtual bool begin() {
    TRACEI();
    is_first = true;
    is_active = true;
    sbc_init(&sbc, 0L);
    return true;
  }

  virtual void end() {
    TRACEI();
    sbc_finish(&sbc);
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

    uint8_t *start = (uint8_t *)data;
    int count = len;
    if (is_first) {
      framelen = firstWrite(data, len);
      LOGI("framelen: %d", framelen);
      // check if we have a valid frame length
      if (isValidFrameLen(framelen)) {
        start = start + framelen;
        count = len - framelen;
        is_first = false;
      }
    }

    if (!is_first) {
      for (int j = 0; j < count; j++) {
        processByte(start[j]);
      }
    }

    return len;
  }


  // Provides the uncompressed length (of the PCM data) in bytes
  int bytesUncompressed() {
    return codeSize();
  }
  /// Provides the compressed length in bytes (after encoding)
  int bytesCompressed() {
    return frameLength();
  }

protected:
  Print *p_print = nullptr;
  sbc_t sbc;
  bool is_first = true;
  bool is_active = false;
  uint8_t *result_buffer = nullptr;
  int result_buffer_size;
  int framelen;
  uint8_t *input_buffer = nullptr;
  int input_pos = 0;

  /// Provides the compressed length in bytes (after encoding)
  int frameLength() { return sbc_get_frame_length(&sbc); }

  // Provides the uncompressed length (of the PCM data) in bytes
  int codeSize() { return sbc_get_codesize(&sbc); }

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
    notifyAudioChange(info);
  }

  bool isValidFrameLen(int len) { return len > 0 && len < 256; }

  /// Determines the framelen
  int firstWrite(const void *data, size_t length) {
    size_t result_len = 0;
    int frame_len = sbc_parse(&sbc, data, length);
    if (isValidFrameLen(frame_len)) {

      // setup audio info
      setupAudioInfo();

      // setup input buffer for subsequent decoding stpes
      setupInputBuffer(frame_len);
    }

    return frame_len;
  }

  void setupInputBuffer(int len) {
    LOGI("input_buffer: %d", len);
    if (input_buffer != nullptr)
      delete[] input_buffer;
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
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class SBCEncoder : public AudioEncoder {
public:
  SBCEncoder(int subbands = 8, int blocks = 16, int bitpool = 32,
             int allocation_method = SBC_AM_LOUDNESS) {
    setSubbands(subbands);
    setBlocks(blocks);
    setBitpool(bitpool);
    setAllocationMethod(allocation_method);
  }

  /// Defines the subbands: Use 4 or 8
  void setSubbands(int subbands) {
    if (subbands == 8 || subbands == 4) {
      this->subbands = subbands;
    } else {
      LOGE("Invalid subbands: %d - using 8", subbands);
      this->subbands = 8;
    }
  }

  /// Defines the number of blocks: valid values (4,8,12,16)
  void setBlocks(int blocks) {
    if (blocks == 16 || blocks == 12 || blocks == 8 || blocks == 4) {
      this->blocks = blocks;
    } else {
      LOGE("Invalid blocks: %d - using 16", blocks);
      this->blocks = 16;
    }
  }

  /// Defines the bitpool (2-86?)
  void setBitpool(int bitpool) { this->bitpool = bitpool; }

  /// Defines the allocation method: Use SBC_AM_LOUDNESS, SBC_AM_SNR
  void setAllocationMethod(int allocation_method) {
    if (allocation_method == SBC_AM_LOUDNESS || allocation_method == SBC_AM_SNR) {
      this->allocation_method = allocation_method;
    } else {
      LOGE("Invalid allocation Method: %d - using SBC_AM_LOUDNESS", allocation_method);
      this->allocation_method = SBC_AM_LOUDNESS;
    }
  }

  /// Restarts the processing
  bool begin() {
    TRACEI();
    is_first = true;
    is_active = setup();
    current_codesize = codeSize();
    buffer.resize(current_codesize);
    result_buffer.resize(frameLength());
    return true;
  }

  /// Ends the processing
  virtual void end() {
    TRACEI();
    sbc_finish(&sbc);
    is_active = false;
  }

  virtual const char *mime() { return "audio/sbc"; }

  virtual void setOutput(Print &out_stream) { p_print = &out_stream; }

  operator bool() { return is_active; }

  virtual size_t write(const uint8_t *data, size_t len) {
    LOGD("write: %d", len);
    if (!is_active) {
      LOGE("inactive");
      return 0;
    }
    if (p_print==nullptr){
      LOGE("output not defined");
      return 0;
    }

    // encode bytes
    for (int j = 0; j < len; j++) {
      processByte(data[j]);
    }

    return len;
  }


  int bytesUncompressed() {
    return codeSize();
  }
  int bytesCompressed() {
    return frameLength();
  }

protected:
  Print *p_print = nullptr;
  sbc_t sbc;
  bool is_first = true;
  bool is_active = false;
  int current_codesize = 0;
  int buffer_pos = 0;
  Vector<uint8_t> buffer{0};
  Vector<uint8_t> result_buffer{0};
  int subbands = 4;
  int blocks = 4;
  int bitpool = 32;
  int allocation_method;

  /// Provides the compressed length in bytes (after encoding)
  int frameLength() { return sbc_get_frame_length(&sbc); }

  /// Provides the uncompressed length (of the PCM data) in bytes
  int codeSize() { return sbc_get_codesize(&sbc); }

  /// Determines audio information and calls sbc_init;
  bool setup() {
    sbc_init(&sbc, 0L);

    if (info.bits_per_sample!=16){
      LOGE("Invalid bits_per_sample: %d", info.bits_per_sample);
      return false;
    }

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

    switch (subbands) {
    case 4:
      sbc.subbands = SBC_SB_4;
      break;
    case 8:
      sbc.subbands = SBC_SB_8;
      break;
    default:
      LOGE("Invalid subbands: %d", subbands);
      return false;
    }

    switch (blocks) {
    case 4:
      sbc.blocks = SBC_BLK_4;
      break;
    case 8:
      sbc.blocks = SBC_BLK_8;
      break;
    case 12:
      sbc.blocks = SBC_BLK_12;
      break;
    case 16:
      sbc.blocks = SBC_BLK_16;
      break;
    default:
      LOGE("Invalid blocks: %d", blocks);
      return false;
    }

    sbc.bitpool = bitpool;
    sbc.allocation = allocation_method;
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
      sbc_encode(&sbc, &buffer[0], current_codesize, &result_buffer[0],
                 result_buffer.size(), &written);
      LOGD("sbc_encode: %d -> %d (buffer: %d))", current_codesize, written,
           result_buffer.size());
      p_print->write(&result_buffer[0], written);
      buffer_pos = 0;
    }
  }
};

} // namespace audio_tools