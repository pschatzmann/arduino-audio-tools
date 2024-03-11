#include "AudioTools.h"
#include "AudioLibs/AudioEspressifFFT.h" // Using Espressif DSP Library

AudioEspressifFFT fftc; 
SineWaveGenerator<int16_t> sineWave(32000);
GeneratedSoundStream<int16_t> in(sineWave);
StreamCopy copier(fftc, in);
AudioInfo info(44100, 1, 16);
float value = 0;

// display fftc result
void fftcResult(AudioFFTBase &fftc) {
  float diff;
  auto result = fftc.result();
  if (result.magnitude > 100) {
    Serial.print(result.frequency);
    Serial.print(" ");
    Serial.print(result.magnitude);
    Serial.print(" => ");
    Serial.print(result.frequencyAsNote(diff));
    Serial.print(" diff: ");
    Serial.print(diff);
    Serial.print(" - time ms ");
    Serial.print(fftc.resultTime() - fftc.resultTimeBegin());
    Serial.println();

  }
}

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // set the frequency
  sineWave.setFrequency(N_B4);

  // Setup sine wave
  auto cfg = in.defaultConfig();
  cfg.copyFrom(info);
  in.begin(cfg);

  // Setup FFT
  auto tcfg = fftc.defaultConfig();
  tcfg.copyFrom(info);
  tcfg.length = 4096;
  tcfg.callback = &fftcResult;
  fftc.begin(tcfg);
}

void loop() { copier.copy(); }