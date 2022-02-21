/**
 * @file streams-i2s-i2s-2.ino
 * @brief Copy audio from I2S to I2S: We use 2 different i2s ports!
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
 
uint16_t sample_rate=44100;
uint16_t channels = 2;
I2SStream in;
I2SStream out; 
StreamCopy copier(out, in); // copies sound into i2s


// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  // change to Warning to improve the quality
  AudioLogger::instance().begin(Serial, AudioLogger::Info); 

  // start I2S in
  Serial.println("starting I2S...");
  auto config_in = in.defaultConfig(RX_MODE);
  config_in.sample_rate = sample_rate; 
  config_in.bits_per_sample = 16;
  config_in.i2s_format = I2S_STD_FORMAT;
  config_in.is_master = true;
  config_in.port_no = 0;
  config_in.pin_ws = 14;
  config_in.pin_bck = 15;
  config_in.pin_data = 16;
  // config_in.fixed_mclk = sample_rate * 256
  // config_in.pin_mck = 2
  in.begin(config_in);

  // start I2S out
  auto config_out = out.defaultConfig(TX_MODE);
  config_out.sample_rate = sample_rate; 
  config_out.bits_per_sample = 16;
  config_out.i2s_format = I2S_STD_FORMAT;
  config_out.is_master = true;
  config_out.port_no = 1;
  config_out.pin_ws = 17;
  config_out.pin_bck = 18;
  config_out.pin_data = 19;
  out.begin(config_out);

  Serial.println("I2S started...");
}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
}
