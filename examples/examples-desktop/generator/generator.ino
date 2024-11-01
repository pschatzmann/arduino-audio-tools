#include "AudioTools.h"
#include "AudioTools/AudioLibs/PortAudioStream.h"

AudioInfo info(44100, 1, 16);
SineWaveGenerator<int16_t> sineWave(32000);               // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> in(sineWave);               // Stream generated from sine wave
PortAudioStream out;                                        // On desktop we use 
StreamCopy copier(out, in); // copy in to out

// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // open output
  auto config = out.defaultConfig();
  config.copyFrom(info);
  out.begin(config);

  // Setup sine wave
  sineWave.begin(info, N_B4);

}

// Arduino loop  
void loop() {
  if (out) copier.copy();
}