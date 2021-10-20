/**
 * @file i2s-serial.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/i2s-serial/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "Arduino.h"
#include "AudioTools.h"

using namespace audio_tools;  

/**
 * @brief We use I2S as input 
 * To test we use a INMP441 microphone. 
 */ 

I2S<int32_t> i2s;
const size_t max_buffer_len = 512;
int32_t buffer[max_buffer_len][2];

// Arduino Setup
void setup(void) {
  Serial.begin(115200);

  // start i2s input with default configuration
  Serial.println("starting I2S...");
  auto config = i2s.defaultConfig(RX_MODE);
  config.sample_rate = 16000;
  i2s.begin(config);
}

// Arduino loop - repeated processing 
void loop() {
  size_t len = i2s.read(buffer, max_buffer_len); 

  for (int j=0;j<len;j++){
    Serial.print((int32_t)buffer[j][0]);
    Serial.print(", ");
    Serial.println((int32_t)buffer[j][1]);
  }
}