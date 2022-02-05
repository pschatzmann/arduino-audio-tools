/**
 * @file streams-adc-a2dp.ino
 * @author Phil Schatzmann
 * @brief We use a mcp6022 analog microphone as input and send the data to A2DP
 * see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-stream/streams-adc-a2dp/README.md
 * @copyright GPLv3
 * #TODO retest is outstanding
 * 
 */
// Add this in your sketch or change the setting in AudioConfig.h
#define USE_A2DP

#include "AudioTools.h"



AnalogAudioStream in; // analog mic
A2DPStream out = A2DPStream::instance() ; // A2DP output - A2DPStream is a singleton!
StreamCopy copier(out, in); // copy in to out
ConverterAutoCenter<int16_t> center(2); // The data has a center of around 26427, so we we need to shift it down to bring the center to 0

// Arduino Setup
void setup(void) {
  Serial.begin(115200);

  // start the bluetooth
  Serial.println("starting A2DP...");
  out.begin(TX_MODE, "MyMusic");  

  // start i2s input with default configuration
  Serial.println("starting ADC...");
  in.begin(in.defaultConfig(RX_MODE));

}

// Arduino loop: just copy the data 
void loop() {
  copier.copy(center);
}