// Simple wrapper for Arduino sketch to compilable with cpp in cmake
#include "Arduino.h"
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecAACFAAC.h"

// sine -> aac (faac) encoder -> hex output
AudioInfo info(44100, 2, 16);
SineGenerator<int16_t> sineWave;
GeneratedSoundStream<int16_t> in(sineWave);
HexDumpOutput out(Serial);
AACEncoderFAAC faac;
EncodedAudioStream encoder(&out, &faac);
StreamCopy copier(encoder, in);
int count = 0;

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  auto cfg = encoder.defaultConfig();
  cfg.copyFrom(info);
  encoder.begin(cfg);

  sineWave.begin(info, N_B4);
}

void loop() {
  copier.copy();
  if (++count > 100) {
    encoder.end();
    exit(0);
  }
}
