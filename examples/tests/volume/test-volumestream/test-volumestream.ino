/**
 * @file test-volumestream.ino
 * @author Phil Schatzmann
 * @brief test for VolumeStream class
 * @copyright GPLv3
 **/
 
#include "AudioTools.h"

AudioInfo audio_info(44100, 2, 16);
SineWaveGenerator<int16_t> sineWave(32000);                // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);    
CsvOutput<int16_t> out(Serial); 
VolumeStream vol(out);     
StreamCopy copier(vol, sound);                             // copies sound to out

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // Define CSV Output
  out.begin(audio_info);
  vol.begin(audio_info);
  vol.setVolume(0.1);

  // Setup sine wave
  sineWave.begin(audio_info, N_B4);
  Serial.println("started...");
}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
}