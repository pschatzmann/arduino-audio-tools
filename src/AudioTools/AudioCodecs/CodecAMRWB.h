#pragma once
#include "AMRWB.h"  // https://github.com/pschatzmann/codec-amr
#include "AudioTools/AudioCodecs/AudioCodecsBase.h"

namespace audio_tools {

/**
 * @brief AMR Wideband Decoder
 * See https://github.com/pschatzmann/codec-amr
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class AMRWBDecoder : public AudioDecoder {
 public:
  /// Default constructor with valid mode values: WB_6_60,WB_8_85,WB_12_65,WB_14_25,
  /// WB_15_85,WB_18_25,WB_19_85,WB_23_05,WB_23_85 (e.g. AMRWB::Mode::WB_6_60)
  AMRWBDecoder(AMRWB::Mode mode) {
    setMode(mode);
    info.channels = 1;
    info.sample_rate = 16000;
  }

  ~AMRWBDecoder() override = default;

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
  int blickSize() { return amr.getEncodedFrameSizeBytes(); }

  /// Provides the frame size (size of decoded frame)
  int frameSize() { return amr.getFrameSizeSamples() * sizeof(int16_t); }

  void setMode(AMRWB::Mode mode) {
    this->mode = mode;
    amr.setMode(mode);
  }

  operator bool() override { return getOutput() != nullptr; }

 protected:
  AMRWB amr;
  AMRWB::Mode mode;
  SingleBuffer<uint8_t> buffer{0};
};

/**
 * @brief AMR Wideband Encoder
 * See https://github.com/pschatzmann/codec-amr
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class AMRWBEncoder : public AudioEncoder {
 public:
  /// Default constructor with valid mode values: WB_6_60,WB_8_85,WB_12_65,WB_14_25,
  /// WB_15_85,WB_18_25,WB_19_85,WB_23_05,WB_23_85 (e.g. AMRWB::Mode::WB_6_60)
  AMRWBEncoder(AMRWB::Mode mode) {
    setMode(mode);
    info.channels = 1;
    info.sample_rate = 16000;
  }

  ~AMRWBEncoder() override = default;

  void setMode(AMRWB::Mode mode) {
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
  AMRWB amr;
  AMRWB::Mode mode;
  SingleBuffer<uint8_t> buffer{0};
  Print *p_print = nullptr;
};

}  // namespace audio_tools