// Simple wrapper for Arduino sketch to compilable with cpp in cmake
#include "Arduino.h"
#include "AudioTools.h"
//#include "AudioLibs/PortAudioStream.h"
#include "AudioLibs/LinuxStdio.h"

LinuxStdio out;                                              // Output to Desktop
SineWaveGenerator<int16_t> sine_wave(32000);                                           // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> in_stream(sine_wave);                   // Stream generated from sine wave
StreamCopy copier(out, in_stream);                                // copies sound to out

void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  auto cfg = out.defaultConfig();
  if (!out.begin(cfg)){
    stop();
  }

  sine_wave.begin(cfg.channels, cfg.sample_rate, N_B4);
  in_stream.begin();
}

void loop(){
    copier.copy();
}

