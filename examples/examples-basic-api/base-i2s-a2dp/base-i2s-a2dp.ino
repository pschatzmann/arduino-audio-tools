/**
 * @file base-i2s-a2dp.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/base-i2s-a2dp/README.md
  * @copyright GPLv3
  * #TODO retest is outstanding
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
   uint8_t* data8 = (uint8_t*)data;
   int bytes = len * 4;
   size_t result_len = i2s.readBytes(data8, bytes);
   // we have data only in 1 channel but we want to fill both
   bothChannels.convert(data8, bytes);
   return result_len / 4;
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