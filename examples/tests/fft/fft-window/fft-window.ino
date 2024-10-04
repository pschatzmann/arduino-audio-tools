#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioRealFFT.h" // using RealFFT

AudioRealFFT fft; // or AudioKissFFT
Hann hann;
BufferedWindow buffered(&hann);
SineWaveGenerator<int16_t> sineWave(32000);
GeneratedSoundStream<int16_t> in(sineWave);
StreamCopy copier(fft, in);
AudioInfo info(44100, 1, 16);
float value = 0;

// display fft result
void fftResult(AudioFFTBase &fft) {
  float diff;
  auto result = fft.result();
  if (result.magnitude > 100) {
    Serial.print(result.frequency);
    Serial.print(" ");
    Serial.print(result.magnitude);
    Serial.print(" => ");
    Serial.print(result.frequencyAsNote(diff));
    Serial.print(" diff: ");
    Serial.print(diff);
    Serial.print(" - time ms ");
    Serial.print(fft.resultTime() - fft.resultTimeBegin());
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
  auto tcfg = fft.defaultConfig();
  tcfg.copyFrom(info);
  tcfg.length = 4096;
  tcfg.callback = &fftResult;
  tcfg.window_function = &buffered;
  fft.begin(tcfg);
}

void loop() { copier.copy(); }