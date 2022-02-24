/**
 * @file adc-a2dp.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/adc-a2dp/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
* #TODO retest is outstanding
* 
 */
// Add this in your sketch or change the setting in AudioConfig.h
#define USE_A2DP

#include "Arduino.h"
#include "BluetoothA2DPSource.h"
#include "AudioTools.h"

/**
 * @brief We use a mcp6022 analog microphone as input and send the data to A2DP
 */ 

AnalogAudioStream adc;
BluetoothA2DPSource a2dp_source;
// The data has a center of around 26427, so we we need to shift it down to bring the center to 0
ConverterScaler<int16_t> scaler(1.0, -26427, 32700 );

// callback used by A2DP to provide the sound data
int32_t get_sound_data(Frame* frames, int32_t count) {
    uint8_t *data = (uint8_t*)frames;
    int channels = 2;
    size_t len = count * channels * sizeof(int16_t);
   // the ADC provides data in 16 bits
    size_t result_len = adc.readBytes(data, len);   
    scaler.convert(data, result_len);
    return result_len;
}

// Arduino Setup
void setup(void) {
  Serial.begin(115200);

  // start i2s input with default configuration
  Serial.println("starting I2S-ADC...");
  adc.begin(adc.defaultConfig(RX_MODE));

  // start the bluetooth
  Serial.println("starting A2DP...");
  a2dp_source.start("MyMusic", get_sound_data);  
}

// Arduino loop - repeated processing 
void loop() {
}