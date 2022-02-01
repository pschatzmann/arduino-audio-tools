/**
 * @file streams-a2dp-analog.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-stream/streams-a2dp-analog/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
// Add this in your sketch or change the setting in AudioConfig.h
#define USE_A2DP

#include "AudioTools.h"



A2DPStream in = A2DPStream::instance() ; // A2DP input - A2DPStream is a singleton!
AnalogAudioStream out; 
StreamCopy copier(out, in, 4100); // copy in to out

// Arduino Setup
void setup(void) {
  Serial.begin(115200);

  // start the bluetooth audio receiver
  Serial.println("starting A2DP...");
  in.begin(RX_MODE, "MyReceiver");  

  auto config = out.defaultConfig(TX_MODE);
  config.sample_rate = in.sink().sample_rate(); 
  config.channels = 2;
  config.bits_per_sample = 16;
  out.begin(config);
}

// Arduino loop  
void loop() {
    copier.copy();
}