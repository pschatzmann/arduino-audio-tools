#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioRealFFT.h" // using RealFFT
#include "AudioTools/AudioLibs/MiniAudioStream.h"

AudioInfo info(44100, 2, 16);
AudioRealFFT afft; // or AudioKissFFT
//CsvOutput<int16_t> out(Serial);
MiniAudioStream out;
StreamCopy copier(out, afft);
int bin_idx = 0;

// privide fft data
void fftFillData(AudioFFTBase &fft) {
  fft.clearBins();
  FFTBin bin{1.0f,1.0f};
  assert(fft.setBin(bin_idx, bin));

  // restart from first bin
  if (++bin_idx>=fft.size()) bin_idx = 0;
}

void setup() {
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // Setup FFT
  auto tcfg = afft.defaultConfig(RX_MODE);
  tcfg.copyFrom(info);
  tcfg.length = 1024;
  tcfg.callback = fftFillData;
  afft.begin(tcfg);

  // setup output
  auto ocfg = out.defaultConfig(TX_MODE);
  ocfg.copyFrom(info);
  out.begin(ocfg);
}

void loop() { copier.copy(); }