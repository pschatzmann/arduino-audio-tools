/**
 * @file channel-converter-avg.ino
 * @brief Test calculating average of two channels
 * @author Urs Utzinger
 * @copyright GPLv3
 **/
 
#include "AudioTools.h"

#define BINSIZE 4

AudioInfo                     info1(44100, 1, 16);
AudioInfo                     info2(44100, 2, 16);
AudioInfo                     info_out(44100/BINSIZE, 2, 16);
SineWaveGenerator<int16_t>    sineWave1(32000);                  // subclass of SoundGenerator with max amplitude of 32000
SineWaveGenerator<int16_t>    sineWave2(32000);                  // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound1(sineWave1);                 // stream generated from sine wave1
GeneratedSoundStream<int16_t> sound2(sineWave2);                 // stream generated from sine wave2
InputMerge<int16_t>           imerge;                            // merge two inputs to stereo
Bin                           binner;                            // channel averager
ConverterStream<int16_t>      binned_stream(imerge, binner);     // pipe the sound to the averager
CsvOutput<int16_t>            serial_out(Serial);                // serial output
StreamCopy                    copier(serial_out, binned_stream); // stream the binner output to serial port

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

  // Configure binning
  binner.setChannels(2);
  binner.setBits(16);
  binner.setBinSize(BINSIZE);
  binner.setAverage(true);

  // Define CSV Output
  serial_out.begin(info_out);

}  

// Arduino loop - copy sound to out with conversion
void loop() {
  copier.copy();
}