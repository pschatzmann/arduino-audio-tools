/**
 * @file streams-generator-fft.ino
 * @author Phil Schatzmann
 * @brief generate sound and analyze tone with fft to determine the musical note
 * @copyright GPLv3
 */
 
#include "Arduino.h"
#include "AudioTools.h"
#include "AudioFFT/FFTStream.h"

using namespace audio_tools;  

uint16_t sample_rate=6000;
uint8_t channels = 1;                                      // The stream will have 2 channels 
SineWaveGenerator<int16_t> sineWave(32000);                // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);   // Stream generated from sine wave
FFTStream<int16_t,float> out(1024); 
StreamCopy copier(out, sound);                             // copies sound into i2s

void processFFTResult(FFTStream<int16_t,float> &fft, FFTArray<float> &values){
  static MusicalNotes notes;
  int diff;

  for(int j=0;j<values.size();j++){
    Serial.print("fft -> j: ");
    Serial.print(j);
    Serial.print(", freq: ");
    Serial.print(fft.toFrequency(j));
    Serial.print(", real: ");
    Serial.print(values[j].real());
    Serial.print(", img: ");
    Serial.print(values[j].imag());
    Serial.print(", distance: ");
    Serial.print(fft.amplitude(values, j));
    Serial.print("-> note: ");
    const char* note = notes.note(fft.toFrequency(j), diff);
    Serial.print(note);
    Serial.print(" / diff: ");
    Serial.print(diff);
    Serial.println();
  }

  Serial.print("=> max index: ");
  Serial.println(fft.maxAmplitudeIdx(values));
  Serial.print("=> note: (expected B4)  ");
  Serial.println(fft.note(values, diff));
  Serial.print(" / diff: ");
  Serial.print(diff);

  Serial.println("-----------------------------------------------------");
}

// Arduino Setup
void setup(void) {  
  Serial.begin(115200);
  sineWave.begin(channels, sample_rate, N_B4);
  out.setCallback(processFFTResult);
  out.begin(sineWave.audioInfo(), Hamming);
}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
}

int main(){
  setup();
  while(true) loop();
}