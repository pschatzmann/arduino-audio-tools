/**
 * @file url_raw-I2S_external_dac.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/url_raw-I2S_externel_dac/README.md 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "WiFi.h"
#include "AudioTools.h"

URLStream music;    // Music Stream
I2SStream i2s;// I2S as Stream
StreamCopy copier(i2s, music, 1024); // copy music to i2s


// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // connect to WIFI
  WiFi.begin("network", "pwd");
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(500); 
  }
 
  // open music stream
  music.begin("https://pschatzmann.github.io/Resources/audio/audio-8000.raw");

  // start I2S with external DAC
  Serial.println("\nstarting I2S...");
  auto cfg = i2s.defaultConfig(TX_MODE);
  cfg.sample_rate = 8000;
  i2s.begin(cfg);
}

// Arduino loop - repeated processing: copy input stream to output stream
void loop() {
  int len = copier.copy();
  if (len){
      Serial.print(".");
  } else {
      delay(5000);
      i2s.end();
      Serial.println("\nCopy ended");
      stop();
  }
}
