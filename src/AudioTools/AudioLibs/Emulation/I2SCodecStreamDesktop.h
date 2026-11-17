#pragma once
/**
 * Desktop (Arduino-Emulator, IS_DESKTOP without I2S) version of the
 * I2SCodecStream: the same API, but the audio goes to the sound card of the
 * computer (miniaudio), so that sketches for audio boards (e.g. an
 * AudioBoard of the audio-driver library) also run on the desktop. The codec
 * of the board is not used; the volume is applied to the samples.
 */
#include "AudioBoard.h"  // install audio-driver library
#include "AudioTools/AudioLibs/MiniAudioStream.h"

namespace audio_tools {

using namespace audio_driver;

/**
 * @brief Configuration for the desktop I2SCodecStream
 * @ingroup io
 */
struct I2SCodecConfig : public MiniAudioConfig {
  input_device_t input_device = ADC_INPUT_LINE1;
  output_device_t output_device = DAC_OUTPUT_ALL;
  bool sd_active = true;
  bool sdmmc_active = false;
};

/**
 * @brief I2SCodecStream on the desktop: output to (and input from) the sound
 * card with miniaudio
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SCodecStream : public MiniAudioStream, public VolumeSupport {
 public:
  I2SCodecStream() = default;
  /// The board is ignored: the sound card is used
  I2SCodecStream(AudioBoard &board) { setBoard(board); }
  I2SCodecStream(AudioBoard *board) { setBoard(board); }

  I2SCodecConfig defaultConfig(RxTxMode mode = TX_MODE) {
    I2SCodecConfig result;
    (MiniAudioConfig &)result = MiniAudioStream::defaultConfig(mode);
    return result;
  }

  bool begin(I2SCodecConfig cfg) { return MiniAudioStream::begin(cfg); }
  bool begin() override { return MiniAudioStream::begin(); }

  /// Writes the samples with the volume applied
  size_t write(const uint8_t *data, size_t len) override {
    float factor = is_muted ? 0.0f : volume();
    if (factor >= 1.0f || audioInfo().bits_per_sample != 16) {
      return MiniAudioStream::write(data, len);
    }
    const int16_t *in = (const int16_t *)data;
    size_t samples = len / 2;
    if (buffer.size() < samples) buffer.resize(samples);
    for (size_t j = 0; j < samples; j++) buffer[j] = in[j] * factor;
    size_t result = MiniAudioStream::write((const uint8_t *)buffer.data(), samples * 2);
    return result == samples * 2 ? len : result;
  }

  bool setVolume(float vol) override {
    VolumeSupport::setVolume(vol < 0.0f ? 0.0f : (vol > 1.0f ? 1.0f : vol));
    return true;
  }
  float getVolume() { return volume(); }
  bool setMute(bool mute) {
    is_muted = mute;
    return true;
  }
  bool setMute(bool mute, int line) { return setMute(mute); }
  bool setPAPower(bool active) { return true; }
  bool setInputVolume(float vol) { return true; }

  AudioBoard &board() { return *p_board; }
  void setBoard(AudioBoard &board) { p_board = &board; }
  void setBoard(AudioBoard *board) { p_board = board; }
  bool hasBoard() { return p_board != nullptr; }

 protected:
  AudioBoard *p_board = nullptr;
  bool is_muted = false;
  Vector<int16_t> buffer;
};

}  // namespace audio_tools
