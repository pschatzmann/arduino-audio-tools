/**
 * @file player-sdfat-ffti2s.ino
 * @brief Audio Player with output to I2S and FFT
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */


#include "AudioTools.h"
#include "AudioTools/Disk/AudioSourceSD.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/AudioLibs/AudioRealFFT.h" // or AudioKissFFT or others

const char *startFilePath="/";
const char* ext="mp3";
AudioSourceSD source(startFilePath, ext);
MultiOutput multi_output;
MP3DecoderHelix decoder;
AudioPlayer player(source, multi_output, decoder);
I2SStream i2s; // output to i2s
AudioRealFFT fft; // or AudioKissFFT or others

// display fft result
void fftResult(AudioFFTBase &fft){
    float diff;
    auto result = fft.result();
    if (result.magnitude>100){
        Serial.print(result.frequency);
        Serial.print(" ");
        Serial.print(result.magnitude);  
        Serial.print(" => ");
        Serial.print(result.frequencyAsNote(diff));
        Serial.print( " diff: ");
        Serial.println(diff);
    }
}

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // setup I2S
  auto cfg = i2s.defaultConfig(TX_MODE);
  i2s.begin(cfg);

  // Setup FFT
  auto tcfg = fft.defaultConfig();
  tcfg.copyFrom(cfg);
  tcfg.length = 1024;
  tcfg.callback = &fftResult;
  fft.begin(tcfg);
 
  // Setup Multioutput
  multi_output.add(fft);
  multi_output.add(i2s);

  // setup player
  //source.setFileFilter("*Bob Dylan*");
  player.begin();
}

void loop() {
  player.copy();
}
