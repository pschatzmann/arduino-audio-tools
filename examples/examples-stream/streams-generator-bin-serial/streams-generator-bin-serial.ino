/**
 * @file streams-generator-bin-serial.ino
 * @author Urs Utzinger
 * @brief Reduce samples by binning; which is summing consecutive samples and optionally dividing by the number of samples summed.
 * @copyright GPLv3
 **/
 
#include "AudioTools.h"

AudioInfo                     info(44100, 2, 16);
SineWaveGenerator<int16_t>    sineWave(16000);         // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);         // stream generated from sine wave
Bin                           binner(64, 2, true, 16);  // configure binning with binsize, channels, enable averaging, bits per channel
ConverterStream<int16_t>      binning(sound, binner);  // pipe the sound to the binner
CsvOutput<int16_t>            out(Serial);             // serial output
StreamCopy                    copier(out, binning);    // stream the binner output to serial port

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // Define CSV Output
  out.begin(info);

  // Setup sine wave
  sineWave.begin(info, N_B4);
}

// Arduino loop - copy sound to out with conversion
void loop() {
  copier.copy();
}