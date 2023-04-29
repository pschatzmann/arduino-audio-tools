#include "Arduino.h"
#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"

AudioKitStream out;
//CsvOutput<int24_t> out(Serial);
SineWaveGenerator<int24_t> sine_wave;               // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int24_t> in_stream(sine_wave); // Stream generated from sine wave
StreamCopy copier(out, in_stream);                  // copies sound to out

void setup(){
  Serial.begin(115200);
  delay(2000);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  auto cfg = out.defaultConfig();
  cfg.bits_per_sample = 24;
  out.begin(cfg);
  sine_wave.begin(cfg, N_B4);
  in_stream.begin(cfg);
}


void loop(){
  copier.copy();
}
