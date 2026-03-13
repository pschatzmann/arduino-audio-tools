#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecWAV.h"

// sound -copy-> converter -> encoder -> out
// 8bit wav must be unsigned, so we convert from signed 16bit to unsigned 8bit

AudioInfo info(8000, 1, 16);
SineWaveGenerator<int16_t> sineWave(32000);
GeneratedSoundStream<int16_t> sound(sineWave);
HexDumpOutput out(Serial);
WAVEncoder wav;
EncodedAudioStream encoder(&out, &wav);
NumberFormatConverterStreamT<int16_t, uint8_t> converter(encoder);
StreamCopy copier(converter, sound);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // Setup sine wave
  auto cfgs = sineWave.defaultConfig();
  cfgs.copyFrom(info);
  sineWave.begin(info, N_B4);

  converter.begin(info);
  encoder.begin(converter.audioInfoOut());
  out.begin();

  Serial.println("Test started...");
}

void loop() { copier.copy(); }
