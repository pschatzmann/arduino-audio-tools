/**
 * @file streams-generator-analog.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-stream/streams-generator-analog/README.md 
 * @author Phil Schatzmann
 * @copyright GPLv3
 **/
 
#include "AudioTools.h"
#include "Experiments/FrequencyDetection.h"

AudioInfo info(44100, 1, 16);
SineWaveGenerator<int16_t> sineWave(32000);                // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);             // Stream generated from sine wave
FrequncyAutoCorrelationStream out; 
StreamCopy copier(out, sound);  
MusicalNotes notes;                           // copies sound into i2s
int idx = 0;

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // start the analog output
  out.begin(info);

  // Setup sine wave
  Serial.println("started...");
}

// Arduino loop - copy sound to out 
void loop() {
  sineWave.begin(info, notes.frequency(idx));
  copier.copy();
  Serial.print(notes.frequency(idx));
  Serial.print("->");
  Serial.println(out.frequency(0));

  if (++idx >= notes.frequencyCount()) stop();
}