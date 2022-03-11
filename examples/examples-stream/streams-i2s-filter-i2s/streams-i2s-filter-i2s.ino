/**
 * @file streams-i2s-filter-i2s.ino
 * @brief Copy audio from I2S to I2S using an FIR filter
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
 
uint16_t sample_rate=44100;
uint16_t channels = 2;
I2SStream in;
I2SStream out; 

// copy filtered values
FilteredStream<int16_t, float> filtered(in, channels);  // Defiles the filter as BaseConverter
StreamCopy copier(out, filtered);               // copies sound into i2s

// define FIR filter parameters
float coef[] = { 0.021, 0.096, 0.146, 0.096, 0.021};


// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  // change to Warning to improve the quality
  AudioLogger::instance().begin(Serial, AudioLogger::Info); 

  // setup filters for all available channels
  filtered.setFilter(0, new FIR<float>(coef));
  filtered.setFilter(1, new FIR<float>(coef));

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
  // config_in.pin_mck = 3

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
