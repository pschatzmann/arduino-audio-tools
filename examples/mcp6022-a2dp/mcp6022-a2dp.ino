#include "Arduino.h"
#include "SoundTools.h"
#ifndef A2DP
#error You need to install the A2DP Library
#endif
#include "BluetoothA2DPSource.h"

using namespace sound_tools;  

/**
 * @brief We use a mcp6022 analog microphone as input and send the data to A2DP
 */ 

const int32_t max_buffer_len = 512;
ADC adc;
BluetoothA2DPSource a2dp_source;
int16_t buffer[max_buffer_len][2];

// callback used by A2DP to provide the sound data
int32_t get_sound_data(Channels* data, int32_t len) {
   int req_len = min(max_buffer_len, len);
   // the microphone provides data in int16_t data
   return i2s.read(static_cast<Channels*>(data), req_len);   
}

// Arduino Setup
void setup(void) {
  Serial.begin(115200);

  // start i2s input with default configuration
  Serial.println("starting I2S-ADC...");
  adc.begin(adc.defaultConfig());

  // start the bluetooth
  Serial.println("starting A2DP...");
  a2dp_source.start("RadioPlayer", get_sound_data);  
}

// Arduino loop - repeated processing 
void loop() {
}