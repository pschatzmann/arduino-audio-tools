#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

AudioInfo info(44100, 2, 16);
SineWaveGenerator<int32_t> sineWave;            // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int32_t> sound(sineWave);  // Stream generated from sine wave
auto invert = [](uint8_t* data, size_t bytes) {
  size_t sample_count = bytes / sizeof(int16_t);
  int16_t* data16 = (int16_t*)data;
  for (int j = 0; j < sample_count; j++) {
    data16[j] = -data16[j];
  }
  return bytes;
};
AudioBoardStream out(AudioKitEs8388V1);
CallbackStream cb(out, invert);
StreamCopy copier(cb, sound);  // copies sound into i2s

void setup(void) {
  Serial.begin(115200);
  //AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  // start input sound
  sineWave.begin(info, N_B4);
  cb.begin(info);

  // start I2S in
  auto cfg = out.defaultConfig(TX_MODE);
  cfg.copyFrom(info);
  out.begin(cfg);
  // set AudioKit to full volume
  out.setVolume(0.3);

}

void loop() {
  copier.copy();  // Arduino loop - copy sound to out
}
