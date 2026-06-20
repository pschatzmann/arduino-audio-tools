
#pragma once

#include <stdint.h>
#include <string.h>

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "codec-shine.h"  // https://github.com/pschatzmann/codec-shine

namespace audio_tools {

/**
 * @brief Lean MP3 Encoder using the shine library
 * @note please install the shine library first:
 * https://github.com/pschatzmann/codec-shine
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class MP3EncoderShine : public AudioEncoder {
 public:
  MP3EncoderShine() = default;

  ~MP3EncoderShine() override { end(); }

  // -------------------------
  // AudioInfoSupport
  // -------------------------
  void setAudioInfo(AudioInfo from) override {
    AudioEncoder::setAudioInfo(from);

    if (_shine != nullptr) {
      // we need to restart the encoder
      end();
      begin();
    }
  }

  AudioInfo audioInfo() override { return info; }

  AudioInfo audioInfoOut() override {
    return info;  // MP3 does not change SR in Shine
  }

  // -------------------------
  // AudioWriter
  // -------------------------
  void setOutput(Print& out_stream) override { _out = &out_stream; }

  operator bool() override { return _shine != nullptr && _out != nullptr; }

  bool begin() override {
    if (!_out) return false;

    if (info.bits_per_sample != 16) {
      LOGE("Unsupported bits per sample: %d. Only 16 is supported.",
           info.bits_per_sample);
      return false;
    }

    if (info.channels != 1 && info.channels != 2) {
      LOGE("Unsupported number of channels: %d. Only 1 or 2 are supported.",
           info.channels);
      return false;
    }

    int bitrate = selectBitrateFast(info.sample_rate, _bitrate);
    _config.wave.channels = (channels)info.channels;
    _config.wave.samplerate = info.sample_rate;
    _config.mpeg.mode = (info.channels == 1) ? MONO : STEREO;
    _config.mpeg.bitr = bitrate;

    if (info.channels == 1 && _config.mpeg.mode != MONO) {
      LOGW(
          "Input is mono but Shine encoder is configured for stereo. "
          "Forcing mono mode.");
      _config.mpeg.mode = MONO;
    }

    _shine = shine_initialise(&_config);
    if (!_shine) {
      LOGE(
          "Failed to initialize Shine encoder with sample rate %d, channels "
          "%d, bitrate %d",
          info.sample_rate, info.channels, bitrate);
      return false;
    }
    LOGI(
        "Shine encoder initialized with sample rate %d, channels %d, bitrate "
        "%d",
        info.sample_rate, info.channels, bitrate);

    int samples_per_pass = shine_samples_per_pass(_shine);
    int buffer_size_bytes = samples_per_pass * info.channels * sizeof(int16_t);
    if (!_pcm_buffer.resize(buffer_size_bytes)) {
      LOGE("Failed to resize PCM buffer to %d bytes", buffer_size_bytes);
      return false;
    }

    return true;
  }

  void end() override {
    if (_shine) {
      flush();

      shine_close(_shine);
      _shine = nullptr;
    }
  }

  // -------------------------
  // Main encode path
  // -------------------------
  size_t write(const uint8_t* data, size_t len) override {
    if (!_shine || !_out) return 0;

    for (int j = 0; j < len; j++) {
      _pcm_buffer.write(data[j]);
      if (_pcm_buffer.isFull()) {
        writeMP3();
        _pcm_buffer.clear();
      }
    }
    return len;
  }

  /// Request the bitrate for encoding (in kbps).
  void setBitrate(int br) { _bitrate = br; }
  /// Sets the deemphasis filter for encoding (e.g. for old recordings)
  /// @param deemphasis NONE = 0, MU50_15 = 1, CITT = 3 }
  void setDeemphasis(emph deemphasis) { _config.mpeg.emph = deemphasis; }
  /// Sets the copyright flag for encoding
  void setCopyright(bool copyright) { _config.mpeg.copyright = copyright; }
  /// Sets the original flag for encoding
  void setOriginal(bool original) { _config.mpeg.original = original; }
  /// Sets the mode for encoding (STEREO = 0, JOINT_STEREO = 1, DUAL_CHANNEL =
  /// 2, MONO = 3 )
  void setMode(modes mode) { _config.mpeg.mode = mode; }

  const char* mime() override { return "audio/mpeg"; }

  uint32_t frameDurationUs() override {
    // 1152 samples per frame at sample rate
    return (1152000000ULL / info.sample_rate);
  }

  uint16_t samplesPerFrame() override { return 1152; }

  void flush() {
    // complete buffer with zeros and write the last frame
    while (!_pcm_buffer.isFull())
      _pcm_buffer.write(0);  // pad with zeros until full
    writeMP3();

    int written = 0;
    uint8_t* data = shine_flush(_shine, &written);
    if (written > 0 && data) {
      writeBlocking(_out, data, written);
    }
  }

 protected:
  Print* _out = nullptr;
  shine_t _shine = nullptr;
  int _bitrate = 128;
  shine_config_t _config{};
  SingleBuffer<uint8_t> _pcm_buffer;

  int writeMP3() {
    int written = 0;
    uint8_t* mp3 = shine_encode_buffer_interleaved(
        _shine, (int16_t*)_pcm_buffer.data(), &written);
    if (written > 0) {
      writeBlocking(_out, mp3, written);
    }
    return written;
  }

  // ---------------------------------------------------
  // Fast bitrate selector (zero allocation version)
  // ---------------------------------------------------
  static int selectBitrateFast(int sr, int req) {
    // minimal safe mapping (embedded-friendly)
    if (req <= 0) {
      if (sr <= 12000) return 32;
      if (sr <= 22050) return 64;
      if (sr <= 32000) return 128;
      return 128;
    }

    if (sr <= 12000 && req > 64) return 64;
    if (sr <= 22050 && req > 128) return 128;
    if (sr <= 32000 && req > 256) return 256;

    if (req > 320) return 320;

    // snap to valid MP3 ladder
    const int table[] = {32,  40,  48,  56,  64,  80,  96,
                         112, 128, 160, 192, 224, 256, 320};

    int best = 128;
    for (int i = 0; i < 14; i++) {
      if (req <= table[i]) {
        best = table[i];
        break;
      }
    }

    return best;
  }
};

}  // namespace audio_tools
