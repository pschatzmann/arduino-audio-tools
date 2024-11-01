#include "AudioTools.h"

AudioInfo from_info(44100, 2, 32);
AudioInfo to_info(44100, 2, 16);
SineWaveGenerator<int32_t> sine_wave;                         // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int32_t> in_stream(sine_wave);                  // Stream generated from sine wave
CsvOutput<int16_t> out(Serial);                         // Output to Serial
FormatConverterStream conv(in_stream);
StreamCopy copier(out, conv);                                  // copies sound to out

void setup(){
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);  

  sine_wave.begin(from_info, N_B4);
  in_stream.begin();

  out.begin(to_info);

  conv.begin(from_info, to_info);
  assert(out.audioInfo() == to_info);
  assert(sine_wave.audioInfo() == from_info);
}

void loop(){
    copier.copy();
}