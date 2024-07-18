/**
 * @file channel-converter-bin-diff.ino
 * @author Urs Utzinger
 * @brief On two channels reduce number of samples by binning, then compute difference between two channels
 * @copyright GPLv3
 **/
 
#include "AudioTools.h"

AudioInfo                     info1(44100, 1, 16);
AudioInfo                     info2(44100, 2, 16);
SineWaveGenerator<int16_t>    sineWave1(16000);         // subclass of SoundGenerator with max amplitude of 32000
SineWaveGenerator<int16_t>    sineWave2(16000);         // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound1(sineWave1);         // stream generated from sine wave
GeneratedSoundStream<int16_t> sound2(sineWave2);         // stream generated from sine wave
InputMerge<int16_t>           imerge;
Bin                           binner;                                 // Binning each channel by average length, setup see below
ChannelDiff                   differ;                                 // Pairwise difference between channels, setup see belwo
MultiConverter<int16_t>       converter(binner, differ);              // Converter will combin binner and differ
ConverterStream<int16_t>      converted_stream(imerge, converter);   // pipe the merged sound to the converter
CsvOutput<int16_t>            serial_out(Serial);                  // serial output
StreamCopy                    copier(serial_out, converted_stream); // stream the binner output to serial port

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  while(!Serial);

  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // Merge input to stereo
  imerge.add(sound1);
  imerge.add(sound2);
  imerge.begin(info2);

  binner.setChannels(2);
  binner.setBits(16);
  binner.setBinSize(4);
  binner.setAverage(true);

  // Setup sine wave
  sineWave1.begin(info1, N_B4);
  sineWave1.begin(info1, N_B5);

  // Define CSV Output
  serial_out.begin(info1);

}

// Arduino loop - copy sound to out with conversion
void loop() {
  copier.copy();
}