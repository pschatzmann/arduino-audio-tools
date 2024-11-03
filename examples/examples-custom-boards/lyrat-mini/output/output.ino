/**
 * @file output.ino
 * @author Phil Schatzmann
 * @brief  Demo to output audio on a lyrat-mini board: with headphone detection 
 * active to switch the power amplifier on and off.
 * I2S is used and the ES8311 is initialized via the driver library.
 * @version 0.1
 * @date 2024-11-03
 *
 * @copyright Copyright (c) 2022
 */

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

AudioInfo info(44100, 2, 16);
SineWaveGenerator<int16_t> sineWave(32000);     // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);  // Stream generated from sine wave
AudioBoardStream out(LyratMini);
StreamCopy copier(out, sound);  // copies sound into i2s

// Arduino Setup
void setup(void) {
  // Open Serial
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // start I2S
  Serial.println("starting I2S...");
  auto config = out.defaultConfig(TX_MODE);
  config.copyFrom(info);
  // cfg.i2s_function = PinFunction::CODEC;  // determined automatically
  // cfg.i2s_port_no = 0; // or 1 if 0 is already in use
  out.begin(config);

  // additinal settings
  out.setVolume(0.5);
  out.addHeadphoneDetectionAction();

  // start sine wave
  sineWave.begin(info, N_B4);
  Serial.println("started...");
}

void loop() {
  copier.copy();
  out.processActions();
}