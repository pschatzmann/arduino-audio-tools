/**
 * @file url_raw-I2S_internal_dac.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/url_raw-I2S_internal_dac/README.md
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"

using namespace audio_tools;  

UrlStream music;   // Music Stream
AnalogAudio dac;
StreamCopy streamCopy(dac, music);
ConverterToInternalDACFormat<int16_t> converter;

// Arduino Setup
void setup(void) {
  Serial.begin(115200);

  // open music stream
  music.begin("https://pschatzmann.github.io/arduino-audio-tools/resources/audio-8000.raw");

  // start I2S with internal DAC -> GPIO25 & GPIO26
  Serial.println("starting I2S...");
  I2SConfig cfg = i2s.defaultConfig(TX_MODE);
  cfg.sample_rate = 8000;
  i2s.begin(cfg);
}

// Arduino loop - repeated processing: copy input stream to output stream
void loop() {
  if (!streamCopy.copy(converter)){
      Serial.println("Copy ended");
      dac.end();
      music.end();
      stop();
  }
}