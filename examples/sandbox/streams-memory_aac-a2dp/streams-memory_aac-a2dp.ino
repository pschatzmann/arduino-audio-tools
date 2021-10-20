/**
 * @file streams-memory_aac-a2dp.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/streams-memory_aac-a2dp/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */

// Add this in your sketch or change the setting in AudioConfig.h
#define USE_A2DP

#include "AudioTools.h"
#include "AudioA2DP.h"
#include "audio.h"

using namespace audio_tools;  

//recorded with 2 channels at 44100 hz
MemoryStream in(gs_16b_2c_44100hz_aac, gs_16b_2c_44100hz_aac_len);

A2DPStream a2dp = A2DPStream::instance() ;  // A2DP input - A2DPStream is a singleton!
AACDecoder decoder(a2dp);                   // decode AAC to pcm and send it to a2dp
AudioOutputStream out(decoder);             // output to decoder
StreamCopy copier(out, in);                 // copy in to out

// Arduino Setup
void setup(void) {
  Serial.begin(115200);

  // We send the sound signal via A2DP - so we conect to the MyMusic Bluetooth Speaker
  a2dp.begin(TX_MODE, "MyMusic");

  Serial.println("A2DP is connected now...");
}

// Arduino loop  
void loop() {
  if (out)
    copier.copy();
}