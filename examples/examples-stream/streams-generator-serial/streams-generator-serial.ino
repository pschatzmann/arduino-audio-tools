/**
 * @file streams-generator-serial.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-stream/streams-generator-serial/README.md 
 * @copyright GPLv3
 **/
 
#include "AudioTools.h"


AudioInfo audio_info(44100, 2, 16);
SineWaveGenerator<int16_t> sineWave(32000);                // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);             // Stream generated from sine wave
CsvOutput<int16_t> out(Serial); 
StreamCopy copier(out, sound);                             // copies sound to out

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  //AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // Define CSV Output
  out.begin(audio_info);

  // Setup sine wave
  sineWave.begin(audio_info, N_B4);
  Serial.println("started...");
}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
}