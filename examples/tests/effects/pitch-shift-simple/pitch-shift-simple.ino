 
#include "AudioTools.h"

int pitch_buffer_size = 256;
float pitch_shift = 1.5;
AudioInfo info(44100, 1, 16);
SineWaveGenerator<int16_t> sineWave(32000);                // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);             // Stream generated from sine wave
CsvOutput<int16_t> out(Serial); 
PitchShiftOutput<int16_t, VariableSpeedRingBufferSimple<int16_t>> pitchShift(out);
//PitchShiftOutput<int16_t, VariableSpeedRingBuffer180<int16_t>> pitchShift(out);
//PitchShiftOutput<int16_t, VariableSpeedRingBuffer<int16_t>> pitchShift(out);
StreamCopy copier(pitchShift, sound);                       // copies sound to out

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // Define CSV Output
  out.begin(info);

  auto pcfg = pitchShift.defaultConfig();
  pcfg.copyFrom(info);
  pcfg.pitch_shift = pitch_shift;
  pcfg.buffer_size = pitch_buffer_size;
  pitchShift.begin(pcfg);

  // Setup sine wave
  sineWave.begin(info, N_B4);
  Serial.println("started...");
}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
}