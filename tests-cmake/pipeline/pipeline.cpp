// Simple wrapper for Arduino sketch to compilable with cpp in cmake
#include "AudioTools.h"
#include "AudioLibs/MiniAudioStream.h"
#include "AudioLibs/StdioStream.h"

int16_t AR1[] = { 0, 4560, 9031, 13327, 17363, 21062, 24350, 27165, 29450, 31163, 32269, 32747, 32587, 31793, 30381, 28377, 25820, 22761, 19259, 15383, 11206, 6812, 2285, -2285, -6812, -11206, -15383, -19259, -22761, -25820, -28377, -30381, -31793, -32587, -32747, -32269, -31163, -29450, -27165, -24350, -21062, -17363, -13327, -9031, -4560 };

AudioInfo info(44100, 2, 16);
GeneratorFromArray<int16_t> wave(AR1, 0, false);
GeneratedSoundStream<int16_t> snd(wave);  
Pipeline pip;
ResampleStream resample;
VolumeStream volume;
ChannelFormatConverterStream channels;
NumberFormatConverterStream bits;
MiniAudioStream out;                         // Output to MiniAudioStream
//I2SStream i2s;
StreamCopy copier(out, pip);

// Arduino Setup
void setup(void) {
  //Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  out.begin();
  resample.setStepSize(0.4f);
  volume.setVolume(0.5);
  channels.setToChannels(1);
  bits.setToBits(32);
  copier.setSynchAudioInfo(true);

  pip.setInput(snd);
  pip.add(resample);
  pip.add(volume);
  pip.add(channels);
  pip.add(bits);
  //pip.addNotifyAudioChange(out);
  Serial.println("*** begin ***");
  pip.begin(info);

}

void loop() {
  copier.copy();
}  