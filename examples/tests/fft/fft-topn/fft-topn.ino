
#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioRealFFT.h" // using RealFFT

AudioRealFFT fft; // or AudioKissFFT
SineWaveGenerator<int16_t> sineWave(32000);
GeneratedSoundStream<int16_t> in(sineWave);
StreamCopy copier(fft, in);
AudioInfo info(44100, 1, 16);
float value = 0;
static const int N = 5;

// check fft result
void fftResult(AudioFFTBase &fft) {
    // build result array
    AudioFFTResult all[fft.size()];
    for (int j=0;j<fft.size();j++){
        all[j].bin = j;
        all[j].magnitude = fft.magnitude(j);
        all[j].frequency = fft.frequency(j);
    }
    // sort descending
    std::sort(all, all + fft.size(), [](AudioFFTResult a, AudioFFTResult b){ return a.magnitude > b.magnitude; });

    // get top 5
    AudioFFTResult topn[N];
    fft.resultArray(topn);

    // compare the topn with sorted result
    for (int j=0;j<N;j++){
        Serial.print(all[j].bin);
        Serial.print(":");
        Serial.print(topn[j].bin);
        Serial.print(" ");
        assert(all[j].bin==topn[j].bin);
    }
    Serial.print(" -> TOP: ");
    Serial.println(fft.result().bin);

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
  auto tcfg = fft.defaultConfig();
  tcfg.copyFrom(info);
  tcfg.length = 512;
  tcfg.callback = &fftResult;
  fft.begin(tcfg);
}

void loop() { copier.copy(); }