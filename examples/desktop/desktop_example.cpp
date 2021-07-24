#include "Arduino.h"
#include "AudioTools.h"

using namespace audio_tools;  

typedef int16_t sound_t;                                  // sound will be represented as int16_t (with 2 bytes)
uint16_t sample_rate=44100;
uint8_t channels = 2;                                     // The stream will have 2 channels 
SineWaveGenerator<sound_t> sineWave(32000);               // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<sound_t> in(sineWave, channels);     // Stream generated from sine wave
DefaultStream out = DefaultStream::instance() ;                 // A2DP input - A2DPStream is a singleton!
StreamCopy copier(out, in); // copy in to out

// Arduino Setup
void setup(void) {
  Serial.begin(115200);

  // We send the test signal via A2DP - so we conect to the MyMusic Bluetooth Speaker
  out.begin(TX_MODE, "MyMusic");

  Serial.println("A2DP is connected now...");

  // Setup sine wave
  sineWave.begin(sample_rate, B4);

}

// Arduino loop  
void loop() {
  if (out)
    copier.copy();
}