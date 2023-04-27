#include "AudioTools.h"
#include "AudioCodecs/ContainerOgg.h"

AudioInfo info(24000, 1, 16);
SineWaveGenerator<int16_t> sineWave(
    32000);  // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(
    sineWave);  // Stream generated from sine wave
HexDumpStream out(Serial);
OggContainerEncoder enc;
EncodedAudioStream encoder(out, enc);  // encode and write
StreamCopy copier(encoder, sound);

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // Setup sine wave
  sineWave.begin(info, N_B4);

  // Opus encoder and decoder need to know the audio info
  encoder.begin(info);

  copier.copyN(2);
}

void loop() {}