#pragma once

#include "opus.h"
#include "AudioTools/AudioTypes.h"
#include "Print.h"

#define MAX_PACKET (1500)
#define MAX_FRAME_SAMP (5760)

namespace audio_tools {

/**
 * @brief OpusSettings where the following values are valid:
 *
  int channels[2] = {1, 2};
  int applications[3] = {Opus_APPLICATION_AUDIO, Opus_APPLICATION_VOIP,
                         Opus_APPLICATION_RESTRICTED_LOWDELAY};
  int bitrates[11] = {6000,  12000, 16000,  24000,     32000,           48000,
                      64000, 96000, 510000, Opus_AUTO, Opus_BITRATE_MAX};
  int force_channels[4] = {Opus_AUTO, Opus_AUTO, 1, 2};
  int use_vbr[3] = {0, 1, 1};
  int vbr_constraints[3] = {0, 1, 1};
  int complexities[11] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  int max_bandwidths[6] = {
      Opus_BANDWIDTH_NARROWBAND, Opus_BANDWIDTH_MEDIUMBAND,
      Opus_BANDWIDTH_WIDEBAND,   Opus_BANDWIDTH_SUPERWIDEBAND,
      Opus_BANDWIDTH_FULLBAND,   Opus_BANDWIDTH_FULLBAND};

  int signals[4] = {Opus_AUTO, Opus_AUTO, Opus_SIGNAL_VOICE, Opus_SIGNAL_MUSIC};
  int inband_fecs[3] = {0, 0, 1};
  int packet_loss_perc[4] = {0, 1, 2, 5};
  int lsb_depths[2] = {8, 24};
  int prediction_disabled[3] = {0, 0, 1};
  int use_dtx[2] = {0, 1};
  int frame_sizes_ms_x2[9] = {5,   10,  20,  40, 80,
                              120, 160, 200, 240}; /* x2 to avoid 2.5 ms
**/

struct OpusSettings : public AudioBaseInfo {
  OpusSettings() {
    sample_rate = 48000;
    channels = 2;
    bits_per_sample = 16;
  }
  int application = Opus_APPLICATION_AUDIO;
  int bitrate = Opus_AUTO;
  int force_channel = Opus_AUTO;
  int vbr = 0;
  int vbr_constraint = 0;
  int complexity = 0;
  int max_bandwidth = Opus_BANDWIDTH_MEDIUMBAND;
  int singal = Opus_SIGNAL_MUSIC;
  int inband_fec = 0;
  int packet_loss_perc = 1;
  int lsb_depth = 8;
  int prediction_disabled = 0;
  int use_dtx = 0;
  // int frame_sizes_ms_x2 = 10; /* x2 to avoid 2.5 ms */
};

/**
 * @brief OpusDecoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class OpusDecoder : public AudioDecoder {
public:
  /**
   * @brief Construct a new OpusDecoder object
   */
  OpusDecoder() { LOGD(LOG_METHOD); }

  /**
   * @brief Construct a new OpusDecoder object
   *
   * @param out_stream Output Stream to which we write the decoded result
   */
  OpusDecoder(Print &out_stream) {
    LOGD(LOG_METHOD);
    p_print = &out_stream;
  }

  /// Defines the output Stream
  void setOutputStream(Print &out_stream) override { p_print = &out_stream; }

  void setNotifyAudioChange(AudioBaseInfoDependent &bi) override {
    this->bid = &bi;
  }

  AudioBaseInfo audioInfo() override { return cfg; }

  OpusSettings defaultConfig() { return cfg; }

  void begin(OpusSettings info) {
    LOGD(LOG_METHOD);
    cfg = info;
    if (bid != nullptr) {
      bid->setAudioInfo(cfg);
    }
    begin();
  }

  void begin() override {
    LOGD(LOG_METHOD);
    int err;
    dec = opus_decoder_create(cfg.sample_rate, cfg.channels, &err);
    if (err != Opus_OK) {
      LOGE("opus_decoder_create");
      return;
    }
    active = true;
  }

  void end() override {
    LOGD(LOG_METHOD);
    if (dec) {
      opus_decoder_destroy(dec);
      dec = nullptr;
    }
    active = false;
  }

  size_t write(const void *in_ptr, size_t in_size) {
    if (!active || p_print == nullptr)
      return 0;
    // decode data
    int out_samples = opus_decode(dec, (uint8_t *)in_ptr, in_size,
                                  (opus_int16 *)outbuf, MAX_FRAME_SAMP, 0);
    if (out_samples > 0) {
      p_print->write(outbuf, out_samples * 2);
    }
    return in_size;
  }

  operator boolean() override { return active; }

protected:
  Print *p_print = nullptr;
  AudioBaseInfoDependent *bid = nullptr;
  OpusSettings cfg;
  ::OpusDecoder *dec;
  bool active;
  uint8_t outbuf[MAX_FRAME_SAMP];
};

/**
 * @brief OpusDecoder - Actually this class does no encoding or decoding at
 * all. It just passes on the data.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class OpusEncoder : public AudioEncoder {
public:
  // Empty Constructor - the output stream must be provided with begin()
  OpusEncoder() {}

  // Constructor providing the output stream
  OpusEncoder(Print &out) { p_print = &out; }

  /// Defines the output Stream
  void setOutputStream(Print &out_stream) override { p_print = &out_stream; }

  /// Provides "audio/pcm"
  const char *mime() { return "audio/opus"; }

  /// We actually do nothing with this
  void setAudioInfo(AudioBaseInfo from) override {
    cfg.sample_rate = from.sample_rate;
    cfg.channels = from.channels;
    cfg.bits_per_sample = from.bits_per_sample;
  }

  /// starts the processing using the actual OpusAudioInfo
  void begin() override {
    int err;
    enc = opus_encoder_create(cfg.sample_rate, cfg.channels, cfg.application,
                              &err);
    if (err != Opus_OK) {
      LOGE("opus_encoder_create");
      return;
    }
    is_open = settings();
  }

  void begin(OpusSettings settings) {
    cfg = settings;
    begin();
  }

  /// stops the processing
  void end() override {
    opus_encoder_destroy(enc);
    is_open = false;
  }

  /// Writes PCM data to be encoded as Opus
  size_t write(const void *in_ptr, size_t in_size) {
    if (!is_open || p_print == nullptr)
      return 0;

    int len =
        opus_encode(enc, (opus_int16 *)in_ptr, in_size / 2, packet, MAX_PACKET);
    if (len > 0 && len <= MAX_PACKET) {
      p_print->write(packet, len);
    }

    return in_size;
  }

  operator boolean() override { return is_open; }

  bool isOpen() { return is_open; }

protected:
  Print *p_print = nullptr;
  ::OpusEncoder *enc = nullptr;
  OpusSettings cfg;
  bool is_open = false;
  unsigned char packet[MAX_PACKET + 257];

  bool settings() {
    bool ok = true;
    if (opus_encoder_ctl(enc, Opus_SET_BITRATE(cfg.bitrate)) != Opus_OK) {
      LOGE("invalid bitrate");
      ok = false;
    }
    if (opus_encoder_ctl(enc, Opus_SET_FORCE_CHANNELS(cfg.force_channel)) !=
        Opus_OK) {
      LOGE("invalid force_channel");
      ok = false;
    };
    if (opus_encoder_ctl(enc, Opus_SET_VBR(cfg.vbr)) != Opus_OK) {
      LOGE("invalid vbr");
      ok = false;
    }
    if (opus_encoder_ctl(enc, Opus_SET_VBR_CONSTRAINT(cfg.vbr_constraint)) !=
        Opus_OK) {
      LOGE("invalid vbr_constraint");
      ok = false;
    }
    if (opus_encoder_ctl(enc, Opus_SET_COMPLEXITY(cfg.complexity)) != Opus_OK) {
      LOGE("invalid complexity");
      ok = false;
    }
    if (opus_encoder_ctl(enc, Opus_SET_MAX_BANDWIDTH(cfg.max_bandwidth)) !=
        Opus_OK) {
      LOGE("invalid max_bandwidth");
      ok = false;
    }
    if (opus_encoder_ctl(enc, Opus_SET_SIGNAL(cfg.singal)) != Opus_OK) {
      LOGE("invalid singal");
      ok = false;
    }
    if (opus_encoder_ctl(enc, Opus_SET_INBAND_FEC(cfg.inband_fec)) != Opus_OK) {
      LOGE("invalid inband_fec");
      ok = false;
    }
    if (opus_encoder_ctl(
            enc, Opus_SET_PACKET_LOSS_PERC(cfg.packet_loss_perc)) != Opus_OK) {
      LOGE("invalid pkt_loss");
      ok = false;
    }
    if (opus_encoder_ctl(enc, Opus_SET_LSB_DEPTH(cfg.lsb_depth)) != Opus_OK) {
      LOGE("invalid lsb_depth");
      ok = false;
    }
    if (opus_encoder_ctl(enc, Opus_SET_PREDICTION_DISABLED(
                                  cfg.prediction_disabled)) != Opus_OK) {
      LOGE("invalid pred_disabled");
      ok = false;
    }
    if (opus_encoder_ctl(enc, Opus_SET_DTX(cfg.use_dtx)) != Opus_OK) {
      LOGE("invalid use_dtx");
      ok = false;
    }
    /**
    if (opus_encoder_ctl(enc, Opus_SET_EXPERT_FRAME_DURATION(
                                  cfg.frame_sizes_ms_x2)) != Opus_OK) {
      LOGE("invalid frame_sizes_ms_x2");
      ok = false;
    }
    */
    return ok;
  }
};

} // namespace audio_tools