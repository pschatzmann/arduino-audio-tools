#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

AudioInfo info(44100, 2, 32);
SineWaveGenerator<int32_t> sineWave;                // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int32_t> sound(sineWave);      // Stream generated from sine wave
AudioBoardStream out(AudioKitEs8388V1);
VolumeStream volume(out);
StreamCopy copier(volume, sound); // copies sound into i2s

void setup(void) {  
  Serial.begin(115200);
  //AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  // start input sound
  sineWave.begin(info, N_B4);
   
  // start I2S in
  auto cfg = out.defaultConfig(TX_MODE);
  cfg.copyFrom(info);
  out.begin(cfg);
  // set AudioKit to full volume
  out.setVolume(1.0); 

  // setup volume
  volume.begin(info);
  volume.setVolume(0.0, 0);
  volume.setVolume(0.3, 1);
}

void loop() {
  copier.copy();  // Arduino loop - copy sound to out
}