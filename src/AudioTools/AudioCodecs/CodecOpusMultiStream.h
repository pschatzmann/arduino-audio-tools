#pragma once

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "Print.h"
#include "opus.h"
#include "opus/opus_multistream.h"

#ifndef OPUS_ENC_MAX_BUFFER_SIZE
#define OPUS_ENC_MAX_BUFFER_SIZE 2048
#endif

#ifndef OPUS_DEC_MAX_BUFFER_SIZE
#define OPUS_DEC_MAX_BUFFER_SIZE 4 * 1024
#endif

namespace audio_tools {

/**
 * @brief Defines the channel mapping scenarios for Opus MultiStream: default
 * is OPUS_CHANNEL_MAPPING_COMBINED which treats the channels as a single stream
 * (e.g. stereo). OPUS_CHANNEL_MAPPING_SEPARATE treats each channel as a
 * separate stream (e.g. 2 mono channels). OPUS_CHANNEL_MAPPING_CUSTOM allows
 * for a custom mapping of channels to streams.
 */
enum OpusChannelMapping {
  OPUS_CHANNEL_MAPPING_COMBINED = 0,  // e.g. stereo
  OPUS_CHANNEL_MAPPING_SEPARATE = 1,  // e.g. 2 mono channels
  OPUS_CHANNEL_MAPPING_CUSTOM = 2
};

/**
 * @brief Settings for Opus MultiStream Decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
struct OpusMultiStreamSettings : public AudioInfo {
  OpusMultiStreamSettings() {
    sample_rate = 48000;
    channels = 2;
    bits_per_sample = 16;
  }
  int max_buffer_size = OPUS_DEC_MAX_BUFFER_SIZE;
  int max_buffer_write_size = 512;
  int streams = 1;
  int coupled_streams = 1;
  OpusChannelMapping default_channel_mapping = OPUS_CHANNEL_MAPPING_COMBINED;
  unsigned char mapping[255] = {};

  void setupChannelMapping() {
    switch (default_channel_mapping) {
      case OPUS_CHANNEL_MAPPING_SEPARATE:
        setupSeparateChannelsMapping();
        break;
      case OPUS_CHANNEL_MAPPING_COMBINED:
        setupDefaultMapping();
        break;
      default:
        break;
    }
  }

  void setupDefaultMapping() {
    memset(mapping, 0, sizeof(mapping));
    if (channels == 1) {
      streams = 1;
      coupled_streams = 0;
      mapping[0] = 0;
    } else if (channels == 2) {
      streams = 1;
      coupled_streams = 1;
      mapping[0] = 0;
      mapping[1] = 1;
    } else {
      streams = channels;
      coupled_streams = 0;
      for (int i = 0; i < channels; i++) {
        mapping[i] = i;
      }
    }
  }

  void setupSeparateChannelsMapping() {
    memset(mapping, 0, sizeof(mapping));
    streams = channels;
    coupled_streams = 0;
    for (int i = 0; i < channels; i++) {
      mapping[i] = i;
    }
  }
};

/**
 * @brief Settings for Opus MultiStream Encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
struct OpusMultiStreamEncoderSettings : public OpusMultiStreamSettings {
  OpusMultiStreamEncoderSettings() : OpusMultiStreamSettings() {
    max_buffer_size = OPUS_ENC_MAX_BUFFER_SIZE;
  }
  int application = OPUS_APPLICATION_AUDIO;
  int bitrate = -1;
  int force_channel = -1;
  int vbr = -1;
  int vbr_constraint = -1;
  int complexity = -1;
  int max_bandwidth = -1;
  int signal = -1;
  int inband_fec = -1;
  int packet_loss_perc = -1;
  int lsb_depth = -1;
  int prediction_disabled = -1;
  int use_dtx = -1;
  int frame_sizes_ms_x2 = -1;
};

/**
 * @brief Decoder for the Opus multistream audio format.
 * Supports up to 255 channels by combining multiple Opus streams into a
 * single packet. Each Opus frame must be provided with one write() call.
 *
 * Depends on https://github.com/pschatzmann/arduino-libopus.git
 *
 * @author Phil Schatzmann
 * @ingroup codecs
 * @ingroup decoder
 * @copyright GPLv3
 */
class OpusMultiStreamAudioDecoder : public AudioDecoder {
 public:
  OpusMultiStreamAudioDecoder(bool releaseOnEnd = false)
      : release_on_end(releaseOnEnd) {}

  OpusMultiStreamAudioDecoder(Print& out_stream) {
    TRACED();
    setOutput(out_stream);
  }

  void setOutput(Print& out_stream) override { p_print = &out_stream; }

  AudioInfo audioInfo() override { return cfg; }

  OpusMultiStreamSettings& config() { return cfg; }
  OpusMultiStreamSettings& defaultConfig() { return cfg; }

  bool begin(OpusMultiStreamSettings settings) {
    TRACED();
    AudioDecoder::setAudioInfo(settings);
    cfg = settings;
    notifyAudioChange(cfg);
    return begin();
  }

  bool begin() override {
    TRACED();
    if (!isValidRate(cfg.sample_rate)) {
      LOGE("Sample rate not supported: %d", cfg.sample_rate);
      return false;
    }
    cfg.setupChannelMapping();
    outbuf.resize(cfg.max_buffer_size);
    assert(outbuf.data() != nullptr);

    int err;
    dec = opus_multistream_decoder_create(cfg.sample_rate, cfg.channels,
                                          cfg.streams, cfg.coupled_streams,
                                          cfg.mapping, &err);
    if (err != OPUS_OK || dec == nullptr) {
      LOGE(
          "opus_multistream_decoder_create: %s for sample_rate: %d, "
          "channels:%d, streams:%d, coupled:%d",
          opus_strerror(err), cfg.sample_rate, cfg.channels, cfg.streams,
          cfg.coupled_streams);
      return false;
    }
    active = true;
    return true;
  }

  void end() override {
    TRACED();
    if (dec != nullptr) {
      opus_multistream_decoder_destroy(dec);
      dec = nullptr;
    }
    if (release_on_end) {
      outbuf.resize(0);
    }
    active = false;
  }

  void setAudioInfo(AudioInfo from) override {
    bool channels_changed = info.channels != from.channels;
    AudioDecoder::setAudioInfo(from);
    info = from;
    cfg.sample_rate = from.sample_rate;
    cfg.channels = from.channels;
    cfg.bits_per_sample = from.bits_per_sample;
    if (channels_changed) {
      cfg.setupChannelMapping();
      end();
      begin();
    }
  }

  size_t write(const uint8_t* data, size_t len) override {
    if (!active || p_print == nullptr) return 0;
    LOGD("OpusMultiStreamAudioDecoder::write: %d", (int)len);
    int in_band_forward_error_correction = 0;
    int frame_count = cfg.max_buffer_size / cfg.channels / sizeof(opus_int16);
    int out_samples = opus_multistream_decode(
        dec, (uint8_t*)data, len, (opus_int16*)outbuf.data(), frame_count,
        in_band_forward_error_correction);
    if (out_samples < 0) {
      LOGW("opus_multistream_decode: %s", opus_strerror(out_samples));
    } else if (out_samples > 0) {
      int out_bytes = out_samples * cfg.channels * sizeof(int16_t);
      LOGD("opus_multistream_decode: %d", out_bytes);
      int open = out_bytes;
      int processed = 0;
      while (open > 0) {
        int to_write = std::min(open, cfg.max_buffer_write_size);
        int written = p_print->write(outbuf.data() + processed, to_write);
        open -= written;
        processed += written;
      }
    }
    return len;
  }

  operator bool() override { return active; }

  void setReleaseOnEnd(bool flag) { release_on_end = flag; }

 protected:
  Print* p_print = nullptr;
  OpusMSDecoder* dec = nullptr;
  OpusMultiStreamSettings cfg;
  bool active = false;
  Vector<uint8_t> outbuf{0};
  const uint32_t valid_rates[5] = {8000, 12000, 16000, 24000, 48000};
  bool release_on_end = false;

  bool isValidRate(int rate) {
    for (auto& valid : valid_rates) {
      if (valid == rate) return true;
    }
    return false;
  }
};

/**
 * @brief Encoder for Opus multistream audio.
 * Supports up to 255 channels by combining multiple Opus streams into a
 * single packet. Each fully encoded frame is written to the output stream.
 *
 * @warning The decoder needs a lot of stack space: SET_LOOP_TASK_STACK_SIZE(16
 * * 1024);
 *
 * Depends on https://github.com/pschatzmann/arduino-libopus.git
 *
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class OpusMultiStreamAudioEncoder : public AudioEncoder {
 public:
  OpusMultiStreamAudioEncoder() = default;

  OpusMultiStreamAudioEncoder(Print& out) { setOutput(out); }

  void setOutput(Print& out_stream) override { p_print = &out_stream; }

  const char* mime() override { return "audio/opus"; }

  void setAudioInfo(AudioInfo from) override {
    AudioEncoder::setAudioInfo(from);
    cfg.sample_rate = from.sample_rate;
    cfg.channels = from.channels;
    cfg.bits_per_sample = from.bits_per_sample;
  }

  bool begin() override {
    int err;
    int size =
        getFrameSizeSamples(cfg.sample_rate) * cfg.channels * sizeof(int16_t);
    frame.resize(size);
    assert(frame.data() != nullptr);
    int packet_len =
        OPUS_ENC_MAX_BUFFER_SIZE > 0 ? OPUS_ENC_MAX_BUFFER_SIZE : 512;
    packet.resize(packet_len);
    assert(packet.data() != nullptr);
    enc = opus_multistream_encoder_create(cfg.sample_rate, cfg.channels,
                                          cfg.streams, cfg.coupled_streams,
                                          cfg.mapping, cfg.application, &err);
    if (err != OPUS_OK || enc == nullptr) {
      LOGE(
          "opus_multistream_encoder_create: %s for sample_rate: %d, "
          "channels:%d, streams:%d, coupled:%d",
          opus_strerror(err), cfg.sample_rate, cfg.channels, cfg.streams,
          cfg.coupled_streams);
      return false;
    }
    is_open = settings();
    return is_open;
  }

  OpusMultiStreamEncoderSettings& config() { return cfg; }

  OpusMultiStreamEncoderSettings& defaultConfig() { return cfg; }

  bool begin(OpusMultiStreamEncoderSettings settings) {
    cfg = settings;
    return begin();
  }

  void end() override {
    if (frame_pos > 0) {
      memset(frame.data() + frame_pos, 0, frame.size() - frame_pos);
      encodeFrame();
      frame_pos = 0;
    }
    if (enc != nullptr) {
      opus_multistream_encoder_destroy(enc);
      enc = nullptr;
    }
    is_open = false;
  }

  size_t write(const uint8_t* data, size_t len) override {
    if (!is_open || p_print == nullptr) return 0;
    LOGD("OpusMultiStreamAudioEncoder::write: %d", (int)len);
    for (int j = 0; j < len; j++) {
      encodeByte(data[j]);
    }
    return len;
  }

  operator bool() override { return is_open; }

  bool isOpen() { return is_open; }

  int lookaheadSamples() {
    if (enc == nullptr) return 0;
    opus_int32 samples = 0;
    if (opus_multistream_encoder_ctl(enc, OPUS_GET_LOOKAHEAD(&samples)) !=
        OPUS_OK) {
      return 0;
    }
    return samples > 0 ? samples : 0;
  }

  int frameSizeSamples() { return getFrameSizeSamples(cfg.sample_rate); }

  int pendingSamples() {
    int bytesPerSample = cfg.channels * (int)sizeof(int16_t);
    if (bytesPerSample <= 0) return 0;
    return frame_pos / bytesPerSample;
  }

  uint16_t samplesPerFrame() { return frameSizeSamples(); }

 protected:
  Print* p_print = nullptr;
  OpusMSEncoder* enc = nullptr;
  OpusMultiStreamEncoderSettings cfg;
  bool is_open = false;
  Vector<uint8_t> frame{0};
  Vector<uint8_t> packet{0};
  int frame_pos = 0;

  void encodeByte(uint8_t data) {
    frame[frame_pos++] = data;
    if (frame_pos >= frame.size()) {
      encodeFrame();
      frame_pos = 0;
    }
  }

  void encodeFrame() {
    if (frame.size() > 0) {
      int frames = frame.size() / cfg.channels / sizeof(int16_t);
      LOGD("opus_multistream_encode - frame_size: %d", frames);
      int len = opus_multistream_encode(enc, (opus_int16*)frame.data(), frames,
                                        packet.data(), packet.size());
      if (len < 0) {
        LOGE("opus_multistream_encode: %s", opus_strerror(len));
      } else if (len > 0) {
        LOGD("opus_multistream_encode: %d", len);
        int eff = p_print->write(packet.data(), len);
        if (eff != len) {
          LOGE("encodeFrame data lost: %d->%d", len, eff);
        }
      }
    }
  }

  int getFrameSizeSamples(int sampling_rate) {
    switch (cfg.frame_sizes_ms_x2) {
      case OPUS_FRAMESIZE_2_5_MS:
        return sampling_rate / 400;
      case OPUS_FRAMESIZE_5_MS:
        return sampling_rate / 200;
      case OPUS_FRAMESIZE_10_MS:
        return sampling_rate / 100;
      case OPUS_FRAMESIZE_20_MS:
        return sampling_rate / 50;
      case OPUS_FRAMESIZE_40_MS:
        return sampling_rate / 25;
      case OPUS_FRAMESIZE_60_MS:
        return 3 * sampling_rate / 50;
      case OPUS_FRAMESIZE_80_MS:
        return 4 * sampling_rate / 50;
      case OPUS_FRAMESIZE_100_MS:
        return 5 * sampling_rate / 50;
      case OPUS_FRAMESIZE_120_MS:
        return 6 * sampling_rate / 50;
    }
    return sampling_rate / 100;
  }

  bool settings() {
    bool ok = true;
    if (cfg.bitrate >= 0 &&
        opus_multistream_encoder_ctl(enc, OPUS_SET_BITRATE(cfg.bitrate)) !=
            OPUS_OK) {
      LOGE("invalid bitrate: %d", cfg.bitrate);
      ok = false;
    }
    if (cfg.force_channel >= 0 &&
        opus_multistream_encoder_ctl(
            enc, OPUS_SET_FORCE_CHANNELS(cfg.force_channel)) != OPUS_OK) {
      LOGE("invalid force_channel: %d", cfg.force_channel);
      ok = false;
    }
    if (cfg.vbr >= 0 &&
        opus_multistream_encoder_ctl(enc, OPUS_SET_VBR(cfg.vbr)) != OPUS_OK) {
      LOGE("invalid vbr: %d", cfg.vbr);
      ok = false;
    }
    if (cfg.vbr_constraint >= 0 &&
        opus_multistream_encoder_ctl(
            enc, OPUS_SET_VBR_CONSTRAINT(cfg.vbr_constraint)) != OPUS_OK) {
      LOGE("invalid vbr_constraint: %d", cfg.vbr_constraint);
      ok = false;
    }
    if (cfg.complexity >= 0 &&
        opus_multistream_encoder_ctl(
            enc, OPUS_SET_COMPLEXITY(cfg.complexity)) != OPUS_OK) {
      LOGE("invalid complexity: %d", cfg.complexity);
      ok = false;
    }
    if (cfg.max_bandwidth >= 0 &&
        opus_multistream_encoder_ctl(
            enc, OPUS_SET_MAX_BANDWIDTH(cfg.max_bandwidth)) != OPUS_OK) {
      LOGE("invalid max_bandwidth: %d", cfg.max_bandwidth);
      ok = false;
    }
    if (cfg.signal >= 0 && opus_multistream_encoder_ctl(
                               enc, OPUS_SET_SIGNAL(cfg.signal)) != OPUS_OK) {
      LOGE("invalid signal: %d", cfg.signal);
      ok = false;
    }
    if (cfg.inband_fec >= 0 &&
        opus_multistream_encoder_ctl(
            enc, OPUS_SET_INBAND_FEC(cfg.inband_fec)) != OPUS_OK) {
      LOGE("invalid inband_fec: %d", cfg.inband_fec);
      ok = false;
    }
    if (cfg.packet_loss_perc >= 0 &&
        opus_multistream_encoder_ctl(
            enc, OPUS_SET_PACKET_LOSS_PERC(cfg.packet_loss_perc)) != OPUS_OK) {
      LOGE("invalid pkt_loss: %d", cfg.packet_loss_perc);
      ok = false;
    }
    if (cfg.lsb_depth >= 0 &&
        opus_multistream_encoder_ctl(enc, OPUS_SET_LSB_DEPTH(cfg.lsb_depth)) !=
            OPUS_OK) {
      LOGE("invalid lsb_depth: %d", cfg.lsb_depth);
      ok = false;
    }
    if (cfg.prediction_disabled >= 0 &&
        opus_multistream_encoder_ctl(
            enc, OPUS_SET_PREDICTION_DISABLED(cfg.prediction_disabled)) !=
            OPUS_OK) {
      LOGE("invalid pred_disabled: %d", cfg.prediction_disabled);
      ok = false;
    }
    if (cfg.use_dtx >= 0 && opus_multistream_encoder_ctl(
                                enc, OPUS_SET_DTX(cfg.use_dtx)) != OPUS_OK) {
      LOGE("invalid use_dtx: %d", cfg.use_dtx);
      ok = false;
    }
    if (cfg.frame_sizes_ms_x2 > 0 &&
        opus_multistream_encoder_ctl(
            enc, OPUS_SET_EXPERT_FRAME_DURATION(cfg.frame_sizes_ms_x2)) !=
            OPUS_OK) {
      LOGE("invalid frame_sizes_ms_x2: %d", cfg.frame_sizes_ms_x2);
      ok = false;
    }
    return ok;
  }
};

}  // namespace audio_tools
