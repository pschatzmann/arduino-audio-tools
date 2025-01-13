#pragma once

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "foxen-flac.h"

namespace audio_tools {

#define FOXEN_IN_BUFFER_SIZE 1024 * 2
#define FOXEN_OUT_BUFFER_SIZE 1024 * 4

/**
 * @brief Foxen FLAC Decoder.
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class FLACDecoderFoxen : public AudioDecoder {
 public:
  FLACDecoderFoxen() = default;

  /// Default Constructor
  FLACDecoderFoxen(int maxBlockSize, int maxChannels,
                   bool convertTo16Bits = true) {
    is_convert_to_16 = convertTo16Bits;
    max_block_size = maxBlockSize;
    max_channels = maxChannels;
  };

  /// Destructor - calls end();
  ~FLACDecoderFoxen() { end(); }

  bool begin() {
    TRACEI();
    is_active = false;
    if (flac == nullptr) {
      size_t foxen_size = fx_flac_size(max_block_size, max_channels);
      if (foxen_size > 0) {
        foxen_data.resize(foxen_size);
      }
      if (foxen_data.data() != nullptr) {
        flac = fx_flac_init(foxen_data.data(), max_block_size, max_channels);
      }
    }
    if (flac != nullptr) {
      is_active = true;
    } else {
      LOGE("not enough memory");
      if (is_stop_on_error) stop();
    }
    if (buffer.size() == 0) {
      buffer.resize(in_buffer_size);
      out.resize(FOXEN_OUT_BUFFER_SIZE);
    }

    return is_active;
  }

  void end() {
    TRACEI();
    flush();
    if (flac != nullptr) {
      foxen_data.resize(0);
      flac = nullptr;
    }
    buffer.resize(0);
    out.resize(0);
    is_active = false;
  }

  size_t write(const uint8_t *data, size_t len) override {
    LOGD("write: %d", len);
    // no processing if not active
    if (!is_active) return 0;

    size_t result = buffer.writeArray(data, len);
    LOGD("buffer availabe: %d", buffer.available());

    while (buffer.available() > 0) {
      if (!decode()) break;
    }

    // if the buffer is full we could not decode anything
    if (buffer.available() == buffer.size()) {
      LOGE("Decoder did not consume any data");
      if (is_stop_on_error) stop();
    }

    LOGD("write: %d -> %d", len, result);
    return result;
  }

  void flush() { decode(); }

  operator bool() override { return is_active; }

  /// Defines the input buffer size (default is 2k)
  void setInBufferSize(int size){
    in_buffer_size = size;
  }

  /// Defines the number of 32 bit samples for providing the result (default is 4k)
  void setOutBufferSize(int size){
    out_buffer_size = size;
  }

  /// Defines the maximum FLAC blocksize: drives the buffer allocation
  void setMaxBlockSize(int size){
    max_block_size = size;
  }

  /// Defines the maximum number of channels: drives the buffer allocation
  void setMaxChannels(int ch){
    max_channels = ch;
  }

  /// Select between 16 and 32 bit output: the default is 16 bits
  void set32Bit(bool flag) {
    is_convert_to_16 = !flag; 
  }

 protected:
  fx_flac_t *flac = nullptr;
  SingleBuffer<uint8_t> buffer{0};
  Vector<int32_t> out;
  Vector<uint8_t> foxen_data{0};
  bool is_active = false;
  bool is_convert_to_16 = true;
  bool is_stop_on_error = true;
  int bits_eff = 0;
  int max_block_size = 5 * 1024;
  int max_channels = 2;
  int in_buffer_size = FOXEN_IN_BUFFER_SIZE;
  int out_buffer_size = FOXEN_OUT_BUFFER_SIZE;

  bool decode() {
    TRACED();
    uint32_t out_len = out.size();
    uint32_t buf_len = buffer.available();
    uint32_t buf_len_result = buf_len;
    int rc = fx_flac_process(flac, buffer.data(), &buf_len_result, out.data(),
                             &out_len);
    // assert(out_len <= FOXEN_OUT_BUFFER_SIZE);

    switch (rc) {
      case FLAC_END_OF_METADATA: {
        processMetadata();
      } break;

      case FLAC_ERR: {
        LOGE("FLAC decoder in error state!");
        if (is_stop_on_error) stop();
      } break;

      default: {
        if (out_len > 0) {
          LOGD("Providing data: %d samples", out_len);
          if (is_convert_to_16) {
            write16BitData(out_len);
          } else {
            write32BitData(out_len);
          }
        }
      } break;
    }
    LOGD("processed: %d bytes of %d -> %d samples", buf_len_result, buf_len,
         out_len);
    // removed processed bytes from buffer
    buffer.clearArray(buf_len_result);
    return buf_len_result > 0 || out_len > 0;
  }

  void write32BitData(int out_len) {
    TRACED();
    // write the result to the output destination
    writeBlocking(p_print, (uint8_t *)out.data(), out_len * sizeof(int32_t));
  }

  void write16BitData(int out_len) {
    TRACED();
    // in place convert to 16 bits
    int16_t *out16 = (int16_t *)out.data();
    for (int j = 0; j < out_len; j++) {
      out16[j] = out.data()[j] >> 16;  // 65538;
    }
    // write the result to the output destination
    LOGI("writeBlocking: %d", out_len * sizeof(int16_t));
    writeBlocking(p_print, (uint8_t *)out.data(), out_len * sizeof(int16_t));
  }

  void processMetadata() {
    bits_eff = fx_flac_get_streaminfo(flac, FLAC_KEY_SAMPLE_SIZE);
    int info_blocksize = fx_flac_get_streaminfo(flac, FLAC_KEY_MAX_BLOCK_SIZE);

    LOGI("bits: %d", bits_eff);
    LOGI("blocksize: %d", info_blocksize);
    // assert(bits_eff == 32);
    info.sample_rate = fx_flac_get_streaminfo(flac, FLAC_KEY_SAMPLE_RATE);
    info.channels = fx_flac_get_streaminfo(flac, FLAC_KEY_N_CHANNELS);
    info.bits_per_sample = is_convert_to_16 ? 16 : bits_eff;
    info.logInfo();
    if (info.channels > max_channels) {
      LOGE("max channels too low: %d -> %d", max_channels, info.channels);
      if (is_stop_on_error) stop();
    }
    if (info_blocksize > max_block_size) {
      LOGE("max channels too low: %d -> %d", max_block_size, info_blocksize);
      if (is_stop_on_error) stop();
    }
    notifyAudioChange(info);
  }
};

}  // namespace audio_tools
