/**
 * @file file_raw-internal_dac.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/file_raw-internal_dac/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include <SPI.h>
#include <SD.h>

using namespace audio_tools;  

File sound_file;
I2S<int16_t> i2s;
I2SStream i2s_stream(i2s);
StreamCopy streamCopy(i2s_stream, sound_file, 1024);
const char* file_name = "/audio.raw";
const int sd_ss_pin = 5;


// Arduino Setup
void setup(void) {
  Serial.begin(115200);

  // Setup SD and open file
  SD.begin(sd_ss_pin);
  sound_file = SD.open(file_name, FILE_READ);

  // start I2S with internal DAC -> GPIO25 & GPIO26
  Serial.println("starting I2S...");
  I2SConfig<int16_t> config = i2s.defaultConfig(TX_MODE);
  config.i2s.mode = static_cast<i2s_mode_t>( I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN);
  i2s.begin(config);
}

// Arduino loop - repeated processing 
void loop() {
  if (streamCopy.copy()){
      Serial.print(".");
  } else {
      Serial.println();
      Serial.println("Copy ended");
      delay(10000);
  }
}