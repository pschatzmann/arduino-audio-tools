/**
 * Demo how to use the Mozzi API to provide a stream of int16_t data.
 * Inspired by https://sensorium.github.io/Mozzi/examples/#01.Basics
 * The result is published to a bluetooth speaker. We use the more efficient
 * A2DP base API with a callback
 */
#include "AudioTools.h"
#include "AudioTools/AudioLibs/A2DPStream.h"
#include "AudioTools/AudioLibs/MozziStream.h"
#include <Oscil.h>                // oscillator template
#include <tables/sin2048_int8.h>  // sine table for oscillator

const int sample_rate = 44100;
AudioInfo info(sample_rate, 2, 16);  // bluetooth requires 44100, stereo, 16 bits
BluetoothA2DPSource a2dp_source;
MozziStream mozzi;  // audio source
const int16_t BYTES_PER_FRAME = 4;
// use: Oscil <table_size, update_rate> oscilName (wavetable), look in .h file
// of table #included above
Oscil<SIN2048_NUM_CELLS, sample_rate> aSin(SIN2048_DATA);
// control variable, use the smallest data size you can for anything used in
// audio
byte gain = 255;

// callback used by A2DP to provide the sound data - usually len is 128 2 channel int16 frames
int32_t get_sound_data(uint8_t* data, int32_t size) {
  int32_t result = mozzi.readBytes(data, size);
  //LOGI("get_sound_data %d->%d",size, result);
  return result;
}

// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // setup mozzi
  auto cfg = mozzi.defaultConfig();
  cfg.control_rate = CONTROL_RATE;
  cfg.copyFrom(info);
  mozzi.begin(cfg);

  aSin.setFreq(3320);  // set the frequency

  // start the bluetooth
  Serial.println("starting A2DP...");
  a2dp_source.start_raw("LEXON MINO L", get_sound_data);
  //a2dp_source.set_volume(100);
}

void updateControl() {
  // as byte, this will automatically roll around to 255 when it passes 0
  gain = gain - 3;
}

AudioOutputMozzi updateAudio() {
  // shift back to STANDARD audio range, like /256 but faster
  return (aSin.next() * gain) >> 8;
}

// Arduino loop - repeated processing
void loop() {
  delay(1000);
}