// Simple wrapper for Arduino sketch to compilable with cpp in cmake
#include "AudioTools.h"
#include "AudioLibs/MiniAudioStream.h"
#include "AudioLibs/StdioStream.h"

//LinuxStdio out;                            // Output to stdio on Desktop
MiniAudioStream out;                         // Output to MiniAudioStream
SineWaveGenerator<int16_t> sine_wave;        // subclass of SoundGenerator with max amplitude
GeneratedSoundStream<int16_t> in_stream(sine_wave); // Stream generated from sine wave
StreamCopy copier(out, in_stream);                  // copies sound to out

void setup(){
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  auto cfg = out.defaultConfig(TX_MODE);
  //cfg.sample_rate = 22000;
  if (!out.begin(cfg)){
    stop();
  }

  sine_wave.begin(cfg, N_B4);
  in_stream.begin();
}

void loop(){
    copier.copy();
}

