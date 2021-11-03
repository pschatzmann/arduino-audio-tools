/**
 * @file streams-generator-a2dp.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/streams-generator-a2dp/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */

// Add this in your sketch or change the setting in AudioConfig.h
#define USE_A2DP

#include "AudioTools.h"
#include "AudioA2DP.h"

using namespace audio_tools;  

typedef int16_t sound_t;                                  // sound will be represented as int16_t (with 2 bytes)
uint16_t sample_rate=44100;
uint8_t channels = 2;                                     // The stream will have 2 channels 
SineWaveGenerator<sound_t> sineWave(32000);               // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<sound_t> in(sineWave);               // Stream generated from sine wave
A2DPStream out = A2DPStream::instance() ;                 // A2DP input - A2DPStream is a singleton!
StreamCopy copier(out, in); // copy in to out

// Arduino Setup
void setup(void) {
  Serial.begin(115200);

  // We send the test signal via A2DP - so we conect to the MyMusic Bluetooth Speaker
  out.setVolume(10);
  out.begin(TX_MODE, "MyMusic");

  Serial.println("A2DP is connected now...");

  // Setup sine wave
  sineWave.begin(channels, sample_rate, N_B4);

}

// Arduino loop  
void loop() {
  if (out)
    copier.copy();
}