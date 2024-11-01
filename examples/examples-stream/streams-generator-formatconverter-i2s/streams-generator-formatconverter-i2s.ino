/**
 * @file streams-generator-formatconverter-i2s.ino
 * @brief Demonstrating FormatConverterStream: we can change the sample_rate, channels and bits_per_sample.
 * This demo is using the converter on the input side. You can also use it on the output side as well, which 
 * consumes less memory!
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
 
#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

AudioInfo from(32000,2,32);
AudioInfo to(16000,1,16);
SineWaveGenerator<int32_t> sineWave;                
GeneratedSoundStream<int32_t> sound(sineWave); // Stream generated from sine wave
I2SStream out;   // or any other e.g. AudioBoardStream, CsvOutput<int16_t> out(Serial); 
FormatConverterStream converter(sound);  // or use converter(out)
StreamCopy copier(out, converter);       //        copier(converter, sound);     

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // Setup Input
  sineWave.begin(from, N_B4);
  sound.begin(from);

  // Define Converter
  converter.begin(from, to);

  // Start Output
  Serial.println("starting I2S...");
  auto config = out.defaultConfig(TX_MODE);
  config.copyFrom(to);
  out.begin(config);

  Serial.println("started...");
}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
}