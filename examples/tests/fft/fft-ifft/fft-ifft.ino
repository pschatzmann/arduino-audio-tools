#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioRealFFT.h" // using RealFFT

AudioInfo info(44100, 1, 16);
AudioRealFFT fft; // or AudioKissFFT
Hann hann;
BufferedWindow buffered(&hann);
SineWaveGenerator<int16_t> sineWave(32000);
GeneratedSoundStream<int16_t> in(sineWave);
StreamCopy copier(fft, in);
CsvOutput<int16_t> out(Serial);
StreamCopy copierIFFT(out, fft);
float value = 0;

// display fft result
void fftResult(AudioFFTBase &fft) {
  copierIFFT.copyAll();
}

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // set the frequency
  sineWave.setFrequency(N_B4);

  // Setup sine wave
  auto cfg = in.defaultConfig();
  cfg.copyFrom(info);
  in.begin(cfg);

  // Setup FFT
  auto tcfg = fft.defaultConfig(RXTX_MODE);
  tcfg.window_function = &buffered;
  tcfg.length = 1024;
  tcfg.stride = 512;
  tcfg.callback = fftResult;
  fft.begin(tcfg);

  // setup output
  out.begin(info);
}

void loop() { copier.copy(); }