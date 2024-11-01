/**
 * @file channel-converter-decimate.ino
 * @author Urs Utzinger
 * @brief Reduce samples by binning; which is summing consecutive samples and optionally dividing by the number of samples summed.
 * @copyright GPLv3
 **/
 
#include "AudioTools.h"
#define FACTOR 4

AudioInfo                     info1(44100, 1, 16);
AudioInfo                     info2(44100, 2, 16);
AudioInfo                     info_out(44100/FACTOR, 2, 16);
SineWaveGenerator<int16_t>    sineWave1(16000); // subclass of SoundGenerator with max amplitude of 16000
SineWaveGenerator<int16_t>    sineWave2(16000); // subclass of SoundGenerator with max amplitude of 16000
GeneratedSoundStream<int16_t> sound1(sineWave1); // stream generated from sine wave
GeneratedSoundStream<int16_t> sound2(sineWave2); // stream generated from sine wave
InputMerge<int16_t>           imerge(sound1,sound2);
Decimate                      decimater(FACTOR, 2, 16);  // decimate by FACTOR on each channel
ConverterStream<int16_t>      decimated_stream(imerge, decimater);  // pipe the sounds to the decimater
CsvOutput<int16_t>            serial_out(Serial);  // serial output
StreamCopy                    copier(serial_out, decimated_stream); // stream the decimater output to serial port

// Arduino Setup
void setup(void) {  

 // Open Serial 
  Serial.begin(115200);
  while(!Serial);

  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // Setup sine wave
  sineWave1.begin(info1, N_B4);
  sineWave2.begin(info1, N_B5);

  // Merge input to stereo
  imerge.begin(info2);

  // Define CSV Output
  serial_out.begin(info_out);

 }

// Arduino loop - copy sound to out with conversion
void loop() {
  copier.copy();
}
