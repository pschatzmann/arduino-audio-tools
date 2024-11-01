/**
 * @file streams-i2s-i2s.ino
 * @brief Copy audio from I2S to I2S  - I2S uses 1 i2s port
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
 
AudioInfo info(44100, 2, 16);
I2SStream i2s;
StreamCopy copier(i2s, i2s); // copies sound into i2s


// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  // change to Warning to improve the quality
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info); 

  // start I2S in
  Serial.println("starting I2S...");
  auto config = i2s.defaultConfig(RXTX_MODE);
  config.copyFrom(info); 
  config.i2s_format = I2S_STD_FORMAT;
  config.pin_ws = 14;
  config.pin_bck = 15;
  config.pin_data = 18;
  config.pin_data_rx = 19;
  //config.fixed_mclk = sample_rate * 256;
  // config.pin_mck = 3; // must be 0,1 or 3 - only for ESP_IDF_VERSION_MAJOR >= 4
  i2s.begin(config);

  Serial.println("I2S started...");
}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
}
