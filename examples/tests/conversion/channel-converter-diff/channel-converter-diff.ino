/**
 * @file channel-converter-diff.ino
 * @brief Test calculating parwise difference of channels
 * @author Urs Utzinger
 * @copyright GPLv3
 **/
 
#include "AudioTools.h"

AudioInfo                     info1(44100, 1, 16);
AudioInfo                     info2(44100, 2, 16);
SineWaveGenerator<int16_t>    sineWave1(16000);                    // subclass of SoundGenerator with max amplitude of 32000
SineWaveGenerator<int16_t>    sineWave2(16000);                    // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound1(sineWave1);                   // stream generated from sine wave1
GeneratedSoundStream<int16_t> sound2(sineWave2);                   // stream generated from sine wave2
InputMerge<int16_t>           imerge;                              // merge two inputs to stereo
ChannelDiff                   differ;                            // channel averager
ConverterStream<int16_t>      diffed_stream(imerge, differ);   // pipe the sound to the averager
CsvOutput<int16_t>            serial_out(Serial);                  // serial output
StreamCopy                    copier(serial_out, diffed_stream); // stream the binner output to serial port

// Arduino Setup
void setup(void) {

  // Open Serial 
  Serial.begin(115200);
  while(!Serial); // wait for Serial to be ready

  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // Setup sine waves
  sineWave1.begin(info1, N_B4);
  sineWave2.begin(info1, N_B5);

  // Merge input to stereo
  imerge.add(sound1);
  imerge.add(sound2);
  imerge.begin(info2);

  // Define CSV Output
  serial_out.begin(info1);

}  

// Arduino loop - copy sound to out with conversion
void loop() {
  copier.copy();
}