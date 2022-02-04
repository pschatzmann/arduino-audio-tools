/**
 * @file streams-i2s-i2s.ino
 * @brief Copy audio from I2S to I2S
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
 
#include "AudioTools.h"

uint16_t sample_rate=44100;
uint16_t channels = 2;
I2SStream in;
I2SStream out; 
StreamCopy copier(out, in);                       // copies sound into i2s
ConverterScaler<int16_t> scaler(0.8, 0, 32767);    // change the signal

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // start I2S in
  Serial.println("starting I2S...");
  auto config = in.defaultConfig(RX_MODE);
  config.sample_rate = sample_rate; 
  config.bits_per_sample = 16;
  config.i2s_format = I2S_STD_FORMAT;
  config.is_master = true;
  config.port_no = 0;
  in.begin(config);

  // start I2S out
  auto config_out = out.defaultConfig(TX_MODE);
  config.sample_rate = sample_rate; 
  config.bits_per_sample = 16;
  config.i2s_format = I2S_STD_FORMAT;
  config.is_master = true;
  config.port_no = 1;
  out.begin(config_out);

  Serial.println("I2S started...");
}

// Arduino loop - copy audio to out 
void loop() {
  copier.copy(scaler);
}
