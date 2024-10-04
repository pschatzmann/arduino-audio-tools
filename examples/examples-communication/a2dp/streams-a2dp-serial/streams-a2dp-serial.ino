/**
 * @file streams-a2dp-serial.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-stream/streams-a2dp-serial/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */

#include "AudioTools.h"
#include "AudioTools/AudioLibs/A2DPStream.h"


A2DPStream in;
CsvOutput<int16_t> out(Serial, 2); // ASCII stream as csv 
StreamCopy copier(out, in); // copy in to out

// Arduino Setup
void setup(void) {
  Serial.begin(115200);

  // start the bluetooth audio receiver
  Serial.println("starting A2DP...");
  auto cfg = in.defaultConfig(RX_MODE);
  cfg.name = "MyReceiver";
  in.begin(cfg);  
}

// Arduino loop  
void loop() {
  copier.copy();
}