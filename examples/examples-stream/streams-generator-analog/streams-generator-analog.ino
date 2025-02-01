/**
 * @file streams-generator-analog.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-stream/streams-generator-analog/README.md 
 * @author Phil Schatzmann
 * @copyright GPLv3
 **/
 
#include "AudioTools.h"

AudioInfo info(8000, 1, 16);
SineWaveGeneratorT<int16_t> sineWave(32000);                // subclass of SoundGeneratorT with max amplitude of 32000
GeneratedSoundStreamT<int16_t> sound(sineWave);             // Stream generated from sine wave
AnalogAudioStream out; 
StreamCopy copier(out, sound);                             // copies sound into i2s

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // start the analog output
  auto config = out.defaultConfig(TX_MODE);
  config.copyFrom(info); 
  out.begin(config);

  // Setup sine wave
  sineWave.begin(info, N_B4);
  Serial.println("started...");
}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
}