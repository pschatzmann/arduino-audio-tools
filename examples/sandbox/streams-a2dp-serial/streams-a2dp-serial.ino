/**
 * @file streams-a2dp-serial.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-stream/streams-a2dp-serial/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
// Add this in your sketch or change the setting in AudioConfig.h
#define USE_A2DP

#include "AudioTools.h"



A2DPStream in = A2DPStream::instance() ; // A2DP input - A2DPStream is a singleton!
CsvStream<int16_t> out(Serial, 2); // ASCII stream as csv 
StreamCopy copier(out, in); // copy in to out

// Arduino Setup
void setup(void) {
  Serial.begin(115200);

  // start the bluetooth audio receiver
  Serial.println("starting A2DP...");
  in.begin(RX_MODE, "MyReceiver");  
}

// Arduino loop  
void loop() {
  copier.copy();
}