/**
 * @file streams-generator-i2s.ino
 * @author Phil Schatzmann
 * @brief 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
 
#include "AudioTools.h"
#include "AudioTools/FFTStream.h"

using namespace audio_tools;  

uint16_t sample_rate=44100;
uint8_t channels = 1;                                      // The stream will have 2 channels 
SineWaveGenerator<int16_t> sineWave(32000);                // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);   // Stream generated from sine wave
FFTStream<int16_t,float> out; 
StreamCopy copier(out, sound);                             // copies sound into i2s

void processFFTResult(FFTArray<float>&array){

}

// Arduino Setup
void setup(void) {  
  Serial.begin(115200);
  out.setCallback(processFFTResult);
  out.begin(channels, 1000);
  sineWave.begin(channels, sample_rate, N_B4);
}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
}
