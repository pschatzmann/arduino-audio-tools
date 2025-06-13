#include "AudioTools.h"

AudioInfo info(44100, 1, 16);
// subclass of SoundGenerator with max amplitude of 32000
SineWaveGenerator<int16_t> sineWave(32000);
// Stream generated from sine wave
GeneratedSoundStream<int16_t> sound(sineWave);
CsvOutput<int16_t> out(Serial);
//ResampleStreamT<LinearInterpolaror> resample(out);
ResampleStreamT<BSplineInterpolator> resample(sound);
StreamCopy copier(out, resample);  // copies sound to out

// Arduino Setup
void setup(void) {
  // Open Serial
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // define resample
  auto rcfg = resample.defaultConfig();
  rcfg.copyFrom(info);
  rcfg.step_size = 0.5;
  resample.begin(rcfg);

  // Define CSV Output
  out.begin(info);

  // Setup sine wave
  sineWave.begin(info, N_B4);
  Serial.println("started...");
}

// Arduino loop - copy sound to out
void loop() { copier.copy(); }