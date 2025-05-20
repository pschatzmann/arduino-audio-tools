#pragma once
#include "AMRNB.h"  // https://github.com/pschatzmann/codec-amr
#include "AudioTools/AudioCodecs/AudioCodecsBase.h"

namespace audio_tools {

/**
 * @brief AMR Narrowband Decoder
 * See https://github.com/pschatzmann/codec-amr
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class AMRNBDecoder : public AudioDecoder {
 public:
  /// Default Constructor with valid mode values:
  /// NB_475,NB_515,NB_59,NB_67,NB_74,NB_795,NB_102,NB_122 (e.g.
  /// AMRNB::Mode::NB_475)
  AMRNBDecoder(AMRNB::Mode mode) {
    setMode(mode);
    info.channels = 1;
    info.sample_rate = 8000;
  }

  ~AMRNBDecoder() override = default;

  void setMode(AMRNB::Mode mode) {
    this->mode = mode;
    amr.setMode(mode);
  }

  bool begin() {
    notifyAudioChange(audioInfo());
    buffer.resize(amr.getEncodedFrameSizeBytes());
    return getOutput() != nullptr;
  }

  void setAudioInfo(AudioInfo from) {
    if (from.bits_per_sample != 16) {
      LOGE("Invalid bits per sample: %d", from.bits_per_sample);
    }
    if (from.sample_rate != 8000) {
      LOGE("Invalid sample rate: %d", from.sample_rate);
    }
    if (from.channels != 1) {
      LOGE("Invalid channels: %d", from.channels);
    }
  }

  size_t write(const uint8_t *data, size_t len) override {
    for (size_t j = 0; j < len; j++) {
      buffer.write(data[j]);
      if (buffer.isFull()) {
        int result_samples = amr.getFrameSizeSamples();
        int16_t result[result_samples];
        int size =
            amr.decode(buffer.data(), buffer.size(), result, result_samples);
        if (size > 0) {
          if (getOutput() != nullptr) {
            getOutput()->write((uint8_t *)result, size * sizeof(int16_t));
          }
        }
        buffer.clear();
      }
    }
    return len;
  }

  /// Provides the block size (size of encoded frame)
  int blockSize() {
    amr.setMode(mode);
    return amr.getEncodedFrameSizeBytes();
  }

  /// Provides the frame size (size of decoded frame)
  int frameSize() { return amr.getFrameSizeSamples() * sizeof(int16_t); }

  operator bool() override { return getOutput() != nullptr; }

 protected:
  AMRNB amr;
  AMRNB::Mode mode;
  SingleBuffer<uint8_t> buffer{0};
};

/**
 * @brief AMR NB Encoder
 * See https://github.com/pschatzmann/codec-amr
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class AMRNBEncoder : public AudioEncoder {
 public:
  /// Default Constructor with valid mode values:
  /// NB_475,NB_515,NB_59,NB_67,NB_74,NB_795,NB_102,NB_122 (e.g.
  /// AMRNB::Mode::NB_475)  AMRNBDecoder(AMRNB::Mode mode) {
  AMRNBEncoder(AMRNB::Mode mode) {
    setMode(mode);
    info.channels = 1;
    info.sample_rate = 8000;
  }

  ~AMRNBEncoder() override = default;

  void setMode(AMRNB::Mode mode) {
    this->mode = mode;
    amr.setMode(mode);
  }

  bool begin() {
    buffer.resize(frameSize());
    return getOutput() != nullptr;
  }

  void setAudioInfo(AudioInfo from) {
    if (from.bits_per_sample != 16) {
      LOGE("Invalid bits per sample: %d", from.bits_per_sample);
    }
    if (from.sample_rate != 8000) {
      LOGE("Invalid sample rate: %d", from.sample_rate);
    }
    if (from.channels != 1) {
      LOGE("Invalid channels: %d", from.channels);
    }
  }

  size_t write(const uint8_t *data, size_t len) override {
    for (size_t j = 0; j < len; j++) {
      buffer.write(data[j]);
      if (buffer.isFull()) {
        int result_bytes = blockSize();
        uint8_t result[result_bytes];
        int size =
            amr.encode((int16_t *)buffer.data(),
                       buffer.size() / sizeof(int16_t), result, result_bytes);
        if (size > 0) {
          if (getOutput() != nullptr) {
            getOutput()->write(result, size);
          }
        }
        buffer.clear();
      }
    }
    return len;
  }

  /// Provides the block size (size of encoded frame)
  int blockSize() {
    amr.setMode(mode);
    return amr.getEncodedFrameSizeBytes();
  }

  /// Provides the frame size (size of decoded frame)
  int frameSize() { return amr.getFrameSizeSamples() * sizeof(int16_t); }

  const char *mime() { return "audio/amr"; }

  void setOutput(Print &out_stream) override { p_print = &out_stream; }

  Print *getOutput() { return p_print; }

 protected:
  AMRNB amr;
  AMRNB::Mode mode;
  SingleBuffer<uint8_t> buffer{0};
  Print *p_print = nullptr;
};

}  // namespace audio_tools