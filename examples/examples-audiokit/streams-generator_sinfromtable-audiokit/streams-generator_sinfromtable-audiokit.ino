/**
 * @file streams-generator_sinfromtable-audiokit.ino
 * @brief Tesing SineFromTableT with output on audiokit
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
 
#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

AudioInfo info(32000, 2, 16);
SineFromTableT<int16_t> sineWave(32000);                // subclass of SoundGeneratorT with max amplitude of 32000
GeneratedSoundStreamT<int16_t> sound(sineWave);             // Stream generated from sine wave
AudioBoardStream out(AudioKitEs8388V1);
//CsvOutput out(Serial);
int sound_len=1024;
StreamCopy copier(out, sound, sound_len);                             // copies sound into i2s
int freq = 122;

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // start I2S
  Serial.println("starting I2S...");
  auto config = out.defaultConfig(); //TX_MODE
  config.copyFrom(info); 
  out.begin(config);

  // Setup sine wave
  sineWave.begin(info, freq);
  Serial.println("started...");
}

// Arduino loop - copy sound to out 
void loop() {
  // the length is defined by sound_len
  copier.copy();
  // increase frequency
  freq += 10;
  if (freq>=10000){
    freq = 20;
  }
  sineWave.setFrequency(freq);
}