#pragma once

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "faac.h"

// Default total bitrate in bps for 16 bit stereo @ 44100 Hz; scaled by
// sample rate and channel count when the bitrate is not set via setBitrate().
#ifndef FAAC_DEFAULT_BITRATE
#define FAAC_DEFAULT_BITRATE 128000
#endif

namespace audio_tools {

/**
 * @brief AAC Encoder (AAC-LC and HE-AAC v1) using faac:
 * https://github.com/pschatzmann/codec-faac
 *
 * The encoder consumes PCM in frames of 1024 (LC) or 2048 (HE-AAC) samples per
 * channel, so this class buffers arbitrary write sizes and encodes full frames
 * automatically. By default ADTS is generated: if you want raw AAC frames call
 * setAdts(false) and use getAudioSpecificConfig() to get the ASC.
 * Only 16 bit PCM input is supported.
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AACEncoderFAAC : public AudioEncoder {
 public:
  AACEncoderFAAC() = default;

  AACEncoderFAAC(Print &out_stream) { setOutput(out_stream); }

  ~AACEncoderFAAC() override { end(); }

  /// Defines the output stream
  void setOutput(Print &out_stream) override { p_print = &out_stream; }

  /// Defines the total target bitrate in bps (for all channels). If not
  /// called, the bitrate is derived from FAAC_DEFAULT_BITRATE.
  void setBitrate(uint32_t bps) { bitrate = bps; }

  /// Defines the quantizer quality (1..5000) for VBR: it is only used if the
  /// bitrate is 0
  void setQuality(uint32_t quality) { quant_quality = quality; }

  /// Defines the object type: FAAC_OBJ_AUTO, FAAC_OBJ_LOW (default) or
  /// FAAC_OBJ_HE_AAC_V1
  void setObjectType(faac_object_type type) { object_type = type; }

  /// Defines the rate control: FAAC_RC_AUTO (default), FAAC_RC_VBR,
  /// FAAC_RC_ABR or FAAC_RC_CBR
  void setRateControl(faac_rate_control rc) { rate_control = rc; }

  /// Defines if ADTS headers are generated (default true)
  void setAdts(bool enabled) { use_adts = enabled; }

  bool begin(AudioInfo info) override {
    setAudioInfo(info);
    return begin();
  }

  bool begin() override {
    TRACEI();
    end();
    if (p_print == nullptr) {
      LOGE("Output undefined");
      return false;
    }
    if (info.bits_per_sample != 16) {
      LOGE("bits_per_sample must be 16, got %d", info.bits_per_sample);
      return false;
    }

    faac_params params;
    faac_params_init(&params, sizeof(params));
    params.sample_rate = info.sample_rate;
    params.num_channels = info.channels;
    params.object_type = object_type;
    params.mpeg_version = FAAC_MPEG4;
    params.input_format = FAAC_INPUT_16BIT;
    params.output_format = use_adts ? FAAC_STREAM_ADTS : FAAC_STREAM_RAW;
    params.rate_control = rate_control;
    params.quant_quality = quant_quality;
    // faac expects the bitrate per channel
    uint32_t total_bps =
        bitrate == (uint32_t)-1 ? defaultBitrate(info) : bitrate;
    params.bit_rate = info.channels > 0 ? total_bps / info.channels : 0;

    faac_status rc = faac_encoder_open(&params, &enc);
    if (rc != FAAC_OK) {
      LOGE("faac_encoder_open: %s", faac_strerror(rc));
      enc = nullptr;
      return false;
    }

    enc_info.struct_size = sizeof(enc_info);
    rc = faac_encoder_get_info(enc, &enc_info);
    if (rc != FAAC_OK) {
      LOGE("faac_encoder_get_info: %s", faac_strerror(rc));
      faac_encoder_close(&enc);
      return false;
    }
    LOGI("frame_samples: %d, max_output_bytes: %d",
         (int)enc_info.frame_samples, (int)enc_info.max_output_bytes);

    frame_samples = enc_info.frame_samples * info.channels;
    pcm_buffer.resize(frame_samples);
    out_buffer.resize(enc_info.max_output_bytes);
    pcm_pos = 0;
    return true;
  }

  /// Encodes the remaining data and releases the encoder
  void end() override {
    if (enc == nullptr) return;
    TRACEI();
    // encode the final partial frame
    if (pcm_pos > 0) {
      encode(pcm_buffer.data(), pcm_pos);
      pcm_pos = 0;
    }
    // drain the encoder
    uint32_t written = 0;
    do {
      if (!encode(nullptr, 0, &written)) break;
    } while (written > 0);
    faac_encoder_close(&enc);
    enc = nullptr;
  }

  size_t write(const uint8_t *data, size_t len) override {
    if (enc == nullptr || data == nullptr) return 0;
    const int16_t *samples = (const int16_t *)data;
    size_t sample_count = len / sizeof(int16_t);
    for (size_t j = 0; j < sample_count; j++) {
      pcm_buffer[pcm_pos++] = samples[j];
      if (pcm_pos >= frame_samples) {
        encode(pcm_buffer.data(), pcm_pos);
        pcm_pos = 0;
      }
    }
    return len;
  }

  const char *mime() override { return "audio/aac"; }

  operator bool() override { return enc != nullptr; }

  /// Provides the AudioSpecificConfig (needed for raw output e.g. in a MP4
  /// container). Returns false if not available.
  bool getAudioSpecificConfig(const uint8_t *&data, uint32_t &len) {
    if (enc == nullptr) return false;
    return faac_encoder_asc(enc, &data, &len) == FAAC_OK;
  }

  /// Provides the information of the opened encoder
  faac_encoder_info &encoderInfo() { return enc_info; }

  /// Provides access to the faac encoder handle
  faac_encoder *driver() { return enc; }

 protected:
  faac_encoder *enc = nullptr;
  faac_encoder_info enc_info;
  Print *p_print = nullptr;
  Vector<int16_t> pcm_buffer{0};
  Vector<uint8_t> out_buffer{0};
  size_t frame_samples = 0;
  size_t pcm_pos = 0;
  uint32_t bitrate = (uint32_t)-1;
  uint32_t quant_quality = 0;
  faac_object_type object_type = FAAC_OBJ_LOW;
  faac_rate_control rate_control = FAAC_RC_AUTO;
  bool use_adts = true;

  /// Scales FAAC_DEFAULT_BITRATE (defined for stereo @ 44100 Hz) by sample
  /// rate and channel count.
  static uint32_t defaultBitrate(AudioInfo info) {
    double bps = FAAC_DEFAULT_BITRATE;
    bps = bps * info.channels / 2.0;
    return (uint32_t)(bps * info.sample_rate / 44100.0);
  }

  bool encode(const int16_t *samples, uint32_t sample_count,
              uint32_t *p_written = nullptr) {
    uint32_t written = 0;
    faac_status rc =
        faac_encoder_encode(enc, samples, sample_count, out_buffer.data(),
                            out_buffer.size(), &written);
    if (p_written != nullptr) *p_written = written;
    if (rc != FAAC_OK) {
      LOGE("faac_encoder_encode: %s", faac_strerror(rc));
      return false;
    }
    if (written > 0) {
      size_t result = p_print->write(out_buffer.data(), written);
      if (result != written) {
        LOGW("Output truncated: %d of %d", (int)result, (int)written);
      }
    }
    return true;
  }
};

}  // namespace audio_tools
