#include "Arduino.h"
#include "BluetoothA2DPSource.h"
#include "SoundTools.h"

using namespace sound_tools;  

/**
 * @brief We use a mcp6022 analog microphone as input and send the data to A2DP
 */ 

ADC adc;
BluetoothA2DPSource a2dp_source;
// The data has a center of around 26427, so we we need to shift it down to bring the center to 0
FilterScaler<int16_t> scaler(1.0, -26427, 32700 );

// callback used by A2DP to provide the sound data
int32_t get_sound_data(Channels* data, int32_t len) {
    arrayOf2int16_t *data_arrays = (arrayOf2int16_t *) data;
   // the ADC provides data in 16 bits
    size_t result_len = adc.read(data_arrays, len);   
    scaler.process(data_arrays, result_len);
    return result_len;
}

// Arduino Setup
void setup(void) {
  Serial.begin(115200);

  // start i2s input with default configuration
  Serial.println("starting I2S-ADC...");
  adc.begin(adc.defaultConfig());

  // start the bluetooth
  Serial.println("starting A2DP...");
  a2dp_source.start("MyMusic", get_sound_data);  
}

// Arduino loop - repeated processing 
void loop() {
}