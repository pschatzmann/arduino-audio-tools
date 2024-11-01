/**
 * Mozzi Input example: we get the input from a generator and
 * output the audio via i2s again. In this example we use the 
 * default value range of 0-1023 for the getAudioInput() method.
 * However usually it is easier to define the desired range in
 * the Mozzi configuration object: e.g. cfg.input_range_from = -244;
 * cfg.input_range_to = 243;
 */
#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/AudioLibs/MozziStream.h"

const int sample_rate = 16000;
AudioInfo info(sample_rate, 1, 16);
AudioBoardStream i2s(AudioKitEs8388V1);  // final output of decoded stream
MozziStream mozzi;
SineWaveGenerator<int16_t> sineWave;                // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);             // Stream generated from sine wave

StreamCopy copier(i2s, mozzi);
// use: Oscil <table_size, update_rate> oscilName (wavetable), look in .h file

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // setup mozzi
  auto cfg = mozzi.defaultConfig();
  cfg.control_rate = CONTROL_RATE;
  cfg.copyFrom(info);
  mozzi.begin(cfg);

  // setup data source for mozzi
  mozzi.setInput(sound);
  sineWave.begin(info, N_B4);

  // setup output
  auto out_cfg = i2s.defaultConfig(RXTX_MODE);
  out_cfg.copyFrom(info);
  i2s.begin(out_cfg);
  i2s.setVolume(1.0);
}

void updateControl() {}

AudioOutputMozzi updateAudio() {
  int asig = mozzi.getAudioInput();  // range 0-1023
  asig = asig - 512;                 // now range is -512 to 511
  // output range in STANDARD mode is -244 to 243,
  // so you might need to adjust your signal to suit
  return asig;
}

void loop() { copier.copy(); }