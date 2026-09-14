/**
 * @file audio-out.ino
 * @brief Sine wave playback for the Hosyond 2.8" ESP32-S3 Display (ES8311
 * codec + FM8002E speaker amp) via AudioBoardStream.
 *
 * Dependencies:
 * - https://github.com/pschatzmann/arduino-audio-tools
 * - https://github.com/pschatzmann/arduino-audio-driver
 * @see https://www.lcdwiki.com/2.8inch_ESP32-S3_Display
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

AudioInfo info(44100, 2, 16);
SineWaveGenerator<int16_t> sineWave;
GeneratedSoundStream<int16_t> sound(sineWave);
AudioBoardStream out(ESP32S3HosyondDisplay);
StreamCopy copier(out, sound);  // copies sound into i2s

void setup(void) {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);
  //AudioDriverLogger.begin(Serial, AudioDriverLogLevel::Info);

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
