#include "Arduino.h"
#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

AudioBoardStream out(AudioKitEs8388V1);
//CsvOutput out(Serial);
SineWaveGeneratorT<int24_t> sine_wave;               // subclass of SoundGeneratorT with max amplitude of 32000
GeneratedSoundStreamT<int24_t> in_stream(sine_wave); // Stream generated from sine wave
StreamCopy copier(out, in_stream);                  // copies sound to out

void setup(){
  Serial.begin(115200);
  delay(2000);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  auto cfg = out.defaultConfig();
  cfg.bits_per_sample = 24;
  out.begin(cfg);
  sine_wave.begin(cfg, N_B4);
  in_stream.begin(cfg);
}


void loop(){
  copier.copy();
}
