/**
 * Demo how to use the Mozzi API to provide a stream of int16_t data.
 * Inspired by https://sensorium.github.io/Mozzi/examples/#01.Basics
 */
#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/AudioLibs/MozziStream.h"
#include <Oscil.h>                // oscillator template
#include <tables/sin2048_int8.h>  // sine table for oscillator

const int sample_rate = 16000;
AudioInfo info(sample_rate, 1, 16);
AudioBoardStream i2s(AudioKitEs8388V1);  // audio sink
MozziStream mozzi; // audio source
StreamCopy copier(i2s, mozzi); // copy source to sink
// use: Oscil <table_size, update_rate> oscilName (wavetable), look in .h file
// of table #included above
Oscil<SIN2048_NUM_CELLS, sample_rate> aSin(SIN2048_DATA);
// control variable, use the smallest data size you can for anything used in
// audio
byte gain = 255;

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // setup mozzi
  auto cfg = mozzi.defaultConfig();
  cfg.control_rate = CONTROL_RATE;
  cfg.copyFrom(info);
  mozzi.begin(cfg);

  // setup output
  auto out_cfg = i2s.defaultConfig();
  out_cfg.copyFrom(info);
  i2s.begin(out_cfg);

  // setup mozzi sine
  aSin.setFreq(3320);  // set the frequency
}

void loop() { copier.copy(); }

void updateControl() {
  // as byte, this will automatically roll around to 255 when it passes 0
  gain = gain - 3;
}

AudioOutputMozzi updateAudio() {
  return (aSin.next() * gain) >>
         8;  // shift back to STANDARD audio range, like /256 but faster
}