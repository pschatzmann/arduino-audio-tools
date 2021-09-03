/**
 * @file streams-generator-fft.ino
 * @author Phil Schatzmann
 * @brief generate sound and analyze tone with fft to determine the musical note
 * @copyright GPLv3
 */
 
#include "AudioTools.h"
#include "AudioTools/FFTStream.h"

using namespace audio_tools;  

uint16_t sample_rate=44100;
uint8_t channels = 1;                            // The stream will have 2 channels 
SineWaveGenerator<int16_t> sineWave(32000);      // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);   // Stream generated from sine wave
FFTStream<int16_t,float> out(2000); 
StreamCopy copier(out, sound);                   // copies sound into fft

void processFFTResult(FFTStream<int16_t,float> &fft, FFTArray<float> &values){
  int diff;
  Serial.print("fft -> note: ");
  Serial.print(fft.note(values, diff));
  Serial.print(" - diff: ");
  Serial.println(diff);
}

// Arduino Setup
void setup(void) {  
  Serial.begin(115200);
  sineWave.begin(channels, sample_rate, N_B4);
  out.setCallback(processFFTResult);
  out.begin(sineWave.audioInfo());
}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
}
