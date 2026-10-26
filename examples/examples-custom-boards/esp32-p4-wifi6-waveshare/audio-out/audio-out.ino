/**
 * @file audio-out.ino
 * @brief Sine wave playback for the Waveshare ESP32-P4-WIFI6 onboard
 * ES8311 codec + speaker amp, via I2SCodecStream.
 *
 * Uses arduino-audio-driver's `WaveshareESP32P4WIFI6` board, which
 * defines the codec I2C and I2S pins and enables the speaker amp (GPIO53).
 *
 * Dependencies:
 * - https://github.com/pschatzmann/arduino-audio-tools
 * - https://github.com/pschatzmann/arduino-audio-driver
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioTools/AudioLibs/I2SCodecStream.h"

AudioInfo info(44100, 2, 16);
SineWaveGenerator<int16_t> sineWave;
GeneratedSoundStream<int16_t> sound(sineWave);
I2SCodecStream out(WaveshareESP32P4WIFI6);
StreamCopy copier(out, sound);

void setup(void) {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  Serial.println("starting I2S...");
  auto config = out.defaultConfig(TX_MODE);
  config.copyFrom(info);
  if (!out.begin(config)) {
    Serial.println("error!");
    stop();
  }
  out.setVolume(0.5f);

  sineWave.begin(info, N_B4);
  Serial.println("started...");
}

void loop() { copier.copy(); }
