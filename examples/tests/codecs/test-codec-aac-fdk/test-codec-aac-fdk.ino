#include "AudioTools.h"
#include "AudioCodecs/CodecAACFDK.h"

// test case for sine -> aac encoder -> hex output
AudioInfo info(44100,2,16);
AACEncoderFDK fdk;
SineWaveGenerator<int16_t> sineWave;            
GeneratedSoundStream<int16_t> in(sineWave);     
HexDumpOutput out(Serial);
EncodedAudioStream encoder(&out, &fdk);
StreamCopy copier(encoder, in);     


void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial,AudioLogger::Warning);

  auto cfg = encoder.defaultConfig();
  cfg.copyFrom(info);
  encoder.begin(cfg);

  // start generation of sound
  sineWave.begin(info, N_B4);
}


// copy the data
void loop() {
  copier.copy();
}