/**
 * @file streams-a2dp-i2s.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-stream/streams-a2dp-i2s/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioLibs/AudioA2DP.h"


A2DPStream in; // A2DP input
I2SStream out; 
StreamCopy copier(out, in, 4100); // copy in to out

// Arduino Setup
void setup(void) {
  Serial.begin(115200);

  // start the bluetooth audio receiver
  Serial.println("starting A2DP...");
  in.begin(RX_MODE, "MyReceiver");  

  I2SConfig config = out.defaultConfig(TX_MODE);
  config.sample_rate = in.sink().sample_rate(); 
  config.channels = 2;
  config.bits_per_sample = 16;
  out.begin(config);
}

// Arduino loop  
void loop() {
    copier.copy();
}