// Simple wrapper for Arduino sketch to compilable with cpp in cmake
#include "Arduino.h"
#include "AudioTools.h"
//#include "AudioTools/AudioLibs/PortAudioStream.h"
#include "AudioTools/AudioLibs/StdioStream.h"

//LinuxStdio out;                                     // Output to Desktop
CsvOutput<int24_t> out(Serial);
SineWaveGenerator<int24_t> sine_wave;        // subclass of SoundGenerator with max amplitude
GeneratedSoundStream<int24_t> in_stream(sine_wave); // Stream generated from sine wave
StreamCopy copier(out, in_stream);                  // copies sound to out

void setup(){
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  auto cfg = out.defaultConfig();
  cfg.bits_per_sample = 24;
  if (!out.begin(cfg)){
    stop();
  }

  sine_wave.begin(cfg.channels, cfg.sample_rate, N_B4);
  in_stream.begin();
}

void loop(){
    copier.copy();
}

