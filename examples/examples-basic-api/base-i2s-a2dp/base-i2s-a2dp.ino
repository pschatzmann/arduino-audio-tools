/**
 * @file base-i2s-a2dp.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/base-i2s-a2dp/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

// Add this in your sketch or change the setting in AudioConfig.h
#define USE_A2DP

#include "AudioTools.h"



/**
 * @brief We use a ADS1015 I2S microphone as input and send the data to A2DP
 * Unfortunatly the data type from the microphone (int32_t)  does not match with the required data type by A2DP (int16_t),
 * so we need to convert.
 */ 

BluetoothA2DPSource a2dp_source;
I2S<int32_t> i2s;
ChannelConverter<int32_t> converter(&NumberConverter::convertFrom32To16);
ConverterFillLeftAndRight<int32_t> bothChannels;
const size_t max_buffer_len = 1024;
int32_t buffer[max_buffer_len][2];

// callback used by A2DP to provide the sound data
int32_t get_sound_data(Frame* data, int32_t len) {
   size_t req_len = min(max_buffer_len,(size_t) len);

   // the microphone provides data in int32_t -> we read it into the buffer of int32_t data
   size_t result_len = i2s.read(buffer, req_len);

   // we have data only in 1 channel but we want to fill both
   bothChannels.convert(buffer, result_len);
   
   // convert buffer to int16 for A2DP
   converter.convert(buffer, data, result_len);
   return result_len;
}

// Arduino Setup
void setup(void) {
  Serial.begin(115200);

  // start i2s input with default configuration
  Serial.println("starting I2S...");
  i2s.begin(i2s.defaultConfig(RX_MODE));

  // start the bluetooth
  Serial.println("starting A2DP...");
  a2dp_source.start("MyMusic", get_sound_data);  
}

// Arduino loop - repeated processing 
void loop() {
}