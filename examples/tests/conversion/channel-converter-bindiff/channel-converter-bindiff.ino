/**
 * @file channel-converter-bin-diff.ino
 * @author Urs Utzinger
 * @brief On two channels reduce number of samples by binning, then compute difference between two channels
 * @copyright GPLv3
 **/
 
#include "AudioTools.h"

#define BINSIZE 4

AudioInfo                     info1(44100, 1, 16);
AudioInfo                     info2(44100, 2, 16);
AudioInfo                     info_out(44100/BINSIZE, 1, 16);
SineWaveGenerator<int16_t>    sineWave1(16000);         // subclass of SoundGenerator with max amplitude of 32000
SineWaveGenerator<int16_t>    sineWave2(16000);         // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound1(sineWave1);        // stream generated from sine wave
GeneratedSoundStream<int16_t> sound2(sineWave2);        // stream generated from sine wave
InputMerge<int16_t>           imerge;
ChannelBinDiff                bindiffer;                            // Binning each channel by average length, setup see below
ConverterStream<int16_t>      converted_stream(imerge, bindiffer);  // pipe the merged sound to the converter
CsvOutput<int16_t>            serial_out(Serial);                   // serial output
StreamCopy                    copier(serial_out, converted_stream); // stream the binner output to serial port

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  while(!Serial);

  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Debug); // Info, Warning, Error, Debug

  // Setup sine wave
  sineWave1.begin(info1, N_B4);
  sineWave2.begin(info1, N_B5);

  // Merge input to stereo
  imerge.add(sound1);
  imerge.add(sound2);
  imerge.begin(info2);

  // Setup binning
  bindiffer.setChannels(2);
  bindiffer.setBits(16);
  bindiffer.setBinSize(BINSIZE);
  bindiffer.setAverage(true);

  // Define CSV Output
  serial_out.begin(info_out);

}

// Arduino loop - copy sound to out with conversion
void loop() {
  copier.copy();
}