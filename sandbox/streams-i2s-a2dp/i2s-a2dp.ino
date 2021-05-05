/**
 * @file i2s-a2dp.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/i2s-a2dp/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */


#include "Arduino.h"
#include "AudioTools.h"

using namespace audio_tools;  

I2S<int32_t> i2s;
I2SStream i2sStream(i2s);  // Access I2S as stream
const A2DPStream &a2dpStream = A2DPStream.instance(); // access A2DP as stream
ChannelConverter<int32_t> converter(&convertFrom32To16);
ConverterFillLeftAndRight<int32_t> bothChannels;
StreamCopy copier(i2sStream, a2dpStream, 1024); // copy a2dpStream to i2sStream

// Arduino Setup
void setup(void) {
  Serial.begin(115200);

  // start i2s input with default configuration
  Serial.println("starting I2S...");
  i2s.begin(i2s.defaultConfig(RX_MODE));

  // start the bluetooth
  Serial.println("starting A2DP...");
  a2dpStream.begin("MyMusic");  
}

// Arduino loop - copy data 
void loop() {
  copier.copy(bothChannels, converter)
}