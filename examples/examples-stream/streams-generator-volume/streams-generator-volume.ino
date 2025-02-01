/**
 * @file streams-generator-volume.ino
 * @author Phil Schatzmann
 * @brief Determines the output volume (=amplitude)
 * @copyright GPLv3
 */
 
#include "AudioTools.h"

AudioInfo info(44100, 2, 16);
SineWaveGeneratorT<int16_t> sineWave(32000);                // subclass of SoundGeneratorT with max amplitude of 32000
GeneratedSoundStreamT<int16_t> sound(sineWave);             // Stream generated from sine wave
VolumeMeter out; 
StreamCopy copier(out, sound, 1024*2);                     // copies sound into VolumeMeter

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  while(!Serial);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // start Volume Meter
  out.begin(info);

  // Setup sine wave
  sineWave.begin(info, N_B4);
  Serial.println("started...");
}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
  Serial.print(out.volume()); // overall
  Serial.print(" ");
  Serial.print(out.volume(0)); // left
  Serial.print(" ");
  Serial.println(out.volume(1)); // right
}