/**
 * @file adc-a2dp.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/adc-a2dp/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
#include "Arduino.h"
#include "BluetoothA2DPSource.h"
#include "AudioTools.h"

using namespace audio_tools;  

/**
 * @brief We use a mcp6022 analog microphone as input and send the data to A2DP
 */ 

ADC adc;
ADCStream adcStream(adc);
A2DPStream a2dpStream;
StreamCopy copier(a2dp, adcStream, 512);

// The data has a center of around 26427, so we we need to shift it down to bring the center to 0
ConverterScaler<int16_t> scaler(1.0, -26427, 32700 );

// Arduino Setup
void setup(void) {
  Serial.begin(115200);

  // start i2s input with default configuration
  Serial.println("starting ADC...");
  adc.begin(adc.defaultConfig());

  // start the bluetooth
  Serial.println("starting A2DP...");
  a2dpStream.start("MyMusic");  
}

// Arduino loop - repeated processing 
void loop() {
  copier.copy(scaler);
}