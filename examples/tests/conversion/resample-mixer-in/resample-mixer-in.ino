#include <AudioTools.h>
#include "AudioLibs/AudioBoardStream.h"

int16_t AR1[] = { 0, 4560, 9031, 13327, 17363, 21062, 24350, 27165, 29450, 31163, 32269, 32747, 32587, 31793, 30381, 28377, 25820, 22761, 19259, 15383, 11206, 6812, 2285, -2285, -6812, -11206, -15383, -19259, -22761, -25820, -28377, -30381, -31793, -32587, -32747, -32269, -31163, -29450, -27165, -24350, -21062, -17363, -13327, -9031, -4560 };

AudioInfo info(44100, 1, 16);
GeneratorFromArray<int16_t> wave(AR1, 0, false);
GeneratedSoundStream<int16_t> snd(wave);  // Stream generated from array1
VolumeStream volume(snd);
ResampleStream resample(volume);
InputMixer<int16_t> mixer;
AudioBoardStream i2s(AudioKitEs8388V1);
StreamCopy copier(i2s, mixer);

// Arduino Setup

void setup(void) {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  auto rcfg1 = resample.defaultConfig();
  rcfg1.copyFrom(info);
  rcfg1.step_size = 0.6;
  resample.begin(rcfg1);

  // start I2S internal
  auto config = i2s.defaultConfig(TX_MODE);
  config.copyFrom(info);
  config.use_apll = false;
  i2s.begin(config);

  mixer.add(resample);  //SOUND1
  mixer.begin(info);

  //waveAR1.begin(info);
  snd.begin(info);

  volume.begin(info);
  volume.setVolume(1.0);
}

void loop() {
  copier.copy();
}  