/**
 * @file streams-generator-freq.ino
 * @brief Test frequency detection
 * @author Phil Schatzmann
 * @copyright GPLv3
 **/
 
#include "AudioTools.h"
#include "Experiments/FrequencyDetection.h"

AudioInfo info(16000, 1, 16);
SineWaveGenerator<int16_t> sineWave(32000);                // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);             // Stream generated from sine wave
FrequncyZeroCrossingStream out; // or use FrequncyAutoCorrelationStream
StreamCopy copier(out, sound);  
MusicalNotes notes;                           // copies sound into i2s
int idx = 0;

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // start the analog output
  out.begin(info);

  // Setup sine wave
  Serial.println("started...");
}

// Arduino loop - copy sound to out 
void loop() {
  float freq = notes.frequency(idx);
  sineWave.begin(info, freq);
  copier.copy();

  // print result
  Serial.print(notes.note(freq));
  Serial.print(" ");
  Serial.print(notes.frequency(idx));
  Serial.print("->");
  Serial.print(out.frequency(0));
  Serial.print(" ");
  Serial.println(notes.note(out.frequency(0)));

  if (++idx >= notes.frequencyCount()) stop();
}