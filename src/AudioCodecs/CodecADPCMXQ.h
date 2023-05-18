#pragma once
#include "AudioCodecs/AudioEncoded.h"
#include "adpcm-lib.h"  // https://github.com/pschatzmann/arduino-adpcm-xq

#define DEFAULT_NOISE_SHAPING NOISE_SHAPING_OFF
#define DEFAULT_LOOKAHEAD 0
#define DEFAULT_BLOCKSIZE_POW2 0

namespace audio_tools {

enum class ADPCMNoiseShaping {
  AD_NOISE_SHAPING_OFF = 0,     // flat noise (no shaping)
  AD_NOISE_SHAPING_STATIC = 1,  // first-order highpass shaping
  AD_NOISE_SHAPING_DYNAMIC = 2
};

/**
 * @brief Decoder for ADPCM-XQ. Depends on
 * https://github.com/pschatzmann/arduino-adpcm-xq
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class ADPCMDecoderXQ : public AudioDecoder {
 public:
  ADPCMDecoderXQ() {
    info.sample_rate = 44100;
    info.channels = 2;
    info.bits_per_sample = 16;
  }

  /// set bocksizes as 2^pow: range from 8 to 15
  void setBlockSizePower(int pow) {
    if (pow >= 8 && pow >= 15) {
      block_size_pow2 = pow;
    }
  }

  /// Set look ahead bytes from 0 to 8
  void setLookahead(int value) {
    if (value <= 8) {
      lookahead = value;
    }
  }

  /// Defines the noise shaping
  void setNoiseShaping(ADPCMNoiseShaping ns) { noise_shaping = (int)ns; }

  void begin() override {
    TRACEI();
    current_byte = 0;
    if (adpcm_cnxt == nullptr) {
      adpcm_cnxt = adpcm_create_context(info.channels, lookahead, noise_shaping,
                                        initial_deltas);

      if (block_size_pow2)
        block_size = 1 << block_size_pow2;
      else
        block_size = 256 * info.channels *
                     (info.sample_rate < 11000 ? 1 : info.sample_rate / 11000);

      samples_per_block =
          (block_size - info.channels * 4) * (info.channels ^ 3) + 1;

      pcm_block.resize(samples_per_block * info.channels);
      adpcm_block.resize(block_size);
    }

    if (p_notify != nullptr) {
      p_notify->setAudioInfo(info);
    }
  }

  void end() override {
    TRACEI();
    if (adpcm_cnxt != nullptr) {
      adpcm_free_context(adpcm_cnxt);
      adpcm_cnxt = nullptr;
    }
    pcm_block.resize(0);
    adpcm_block.resize(0);
  }

  virtual void setOutput(Print &out_stream) { p_print = &out_stream; }

  operator bool() override { return adpcm_cnxt != nullptr; }

  virtual size_t write(const void *input_buffer, size_t length) {
    uint8_t *input_buffer8 = (uint8_t *)input_buffer;
    LOGD("write: %d", (int)length);
    for (int j = 0; j < length; j++) {
      adpcm_block[current_byte++] = input_buffer8[j];
      if (current_byte == block_size) {
        decode(current_byte);
        current_byte = 0;
      }
    }
    return length;
  }

 protected:
  int current_byte = 0;
  void *adpcm_cnxt = nullptr;
  Vector<int16_t> pcm_block;
  Vector<uint8_t> adpcm_block;
  int32_t initial_deltas[2] = {0};
  Print *p_print = nullptr;
  int samples_per_block = 0, lookahead = DEFAULT_LOOKAHEAD,
      noise_shaping = (int)DEFAULT_NOISE_SHAPING,
      block_size_pow2 = DEFAULT_BLOCKSIZE_POW2, block_size = 0;

  bool decode(int this_block_adpcm_samples) {
    int result = adpcm_decode_block(pcm_block.data(), adpcm_block.data(),
                                    block_size, info.channels);
    if (result != samples_per_block) {
      LOGE("adpcm_decode_block: %d instead %d", result,
           this_block_adpcm_samples);
      return false;
    }
    int write_size = samples_per_block * info.channels * 2;
    p_print->write((uint8_t *)pcm_block.data(), write_size);
    return true;
  }
};

/**
 * @brief Encoder for ADPCM-XQ - Depends on
 * https://github.com/pschatzmann/arduino-adpcm-xq
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class ADPCMEncoderXQ : public AudioEncoder {
 public:
  ADPCMEncoderXQ() {
    info.sample_rate = 44100;
    info.channels = 2;
    info.bits_per_sample = 16;
  }

  /// set bocksizes as 2^pow: range from 8 to 15
  void setBlockSizePower(int pow) {
    if (pow >= 8 && pow >= 15) {
      block_size_pow2 = pow;
    }
  }

  /// Set look ahead bytes from 0 to 8
  void setLookahead(int value) {
    if (value <= 8) {
      lookahead = value;
    }
  }

  /// Defines the noise shaping
  void setNoiseShaping(ADPCMNoiseShaping ns) { noise_shaping = (int)ns; }

  void begin(AudioInfo info) {
    setAudioInfo(info);
    begin();
  }

  void begin() override {
    TRACEI();

    if (block_size_pow2)
      block_size = 1 << block_size_pow2;
    else
      block_size = 256 * info.channels *
                   (info.sample_rate < 11000 ? 1 : info.sample_rate / 11000);

    samples_per_block =
        (block_size - info.channels * 4) * (info.channels ^ 3) + 1;

    pcm_block.resize(samples_per_block * info.channels);
    adpcm_block.resize(block_size);
    current_sample = 0;
  }

  void end() override {
    TRACEI();
    if (adpcm_cnxt != nullptr) {
      adpcm_free_context(adpcm_cnxt);
      adpcm_cnxt = nullptr;
    }
    pcm_block.resize(0);
    adpcm_block.resize(0);
  }

  const char *mime() override { return "audio/adpcm"; }

  void setAudioInfo(AudioInfo info) override { this->info = info; }

  void setOutput(Print &out_stream) override { p_print = &out_stream; }

  operator bool() override { return adpcm_cnxt != nullptr; }

  size_t write(const void *in_ptr, size_t in_size) override {
    LOGD("write: %d", (int)in_size);
    int16_t *input_buffer = (int16_t *)in_ptr;
    pcm_block_size = samples_per_block * info.channels;
    for (int j = 0; j < in_size / 2; j++) {
      pcm_block[current_sample++] = input_buffer[j];
      if (current_sample == samples_per_block * info.channels) {
        encode();
        current_sample = 0;
      }
    }
    return in_size;
  }

 protected:
  AudioInfo info;
  int current_sample = 0;
  void *adpcm_cnxt = nullptr;
  Vector<int16_t> pcm_block;
  Vector<uint8_t> adpcm_block;
  Print *p_print = nullptr;
  int samples_per_block = 0, lookahead = DEFAULT_LOOKAHEAD,
      noise_shaping = (int)DEFAULT_NOISE_SHAPING,
      block_size_pow2 = DEFAULT_BLOCKSIZE_POW2, block_size = 0, pcm_block_size;
  bool is_first = true;

  bool encode() {
    // if this is the first block, compute a decaying average (in reverse) so
    // that we can let the encoder know what kind of initial deltas to expect
    // (helps initializing index)

    if (adpcm_cnxt == nullptr) {
      is_first = false;
      int32_t average_deltas[2];

      average_deltas[0] = average_deltas[1] = 0;

      for (int i = samples_per_block * info.channels; i -= info.channels;) {
        average_deltas[0] -= average_deltas[0] >> 3;
        average_deltas[0] +=
            abs((int32_t)pcm_block[i] - pcm_block[i - info.channels]);

        if (info.channels == 2) {
          average_deltas[1] -= average_deltas[1] >> 3;
          average_deltas[1] +=
              abs((int32_t)pcm_block[i - 1] - pcm_block[i + 1]);
        }
      }

      average_deltas[0] >>= 3;
      average_deltas[1] >>= 3;

      adpcm_cnxt = adpcm_create_context(info.channels, lookahead, noise_shaping,
                                        average_deltas);
    }

    size_t num_bytes;
    adpcm_encode_block(adpcm_cnxt, adpcm_block.data(), &num_bytes,
                       pcm_block.data(), samples_per_block);

    if (num_bytes != block_size) {
      LOGE(
          "adpcm_encode_block() did not return expected value "
          "(expected %d, got %d)!\n",
          block_size, (int)num_bytes);
      return false;
    }

    p_print->write(adpcm_block.data(), block_size);
    return true;
  }
};

}  // namespace audio_tools

