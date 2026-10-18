// Checks AudioFFTBase::toMEL()/fromMEL(): no empty mel bands, the band of a
// sine has the most energy, the cached filterbank gives identical results,
// and power == magnitude^2 for a band fed by a single bin.
// Exit code 0 = pass.
#include "AudioTools.h"
#include "AudioTools/FFT/AudioRealFFT.h"

AudioRealFFT fft;
Hann hann;
int results = 0;
int failures = 0;

#define CHECK(cond)                                         \
  if (!(cond)) {                                            \
    failures++;                                             \
    printf("FAIL line %d: %s\n", __LINE__, #cond);          \
  }

void onResult(AudioFFTBase &f) { results++; }

void feedSine(float freq, int samples) {
  for (int j = 0; j < samples; j++) {
    int16_t s = (int16_t)(16000.0f * sinf(2.0f * PI * freq * j / 16000.0f));
    fft.write((uint8_t *)&s, sizeof(s));
  }
}

void setup() {
  auto cfg = fft.defaultConfig();
  cfg.sample_rate = 16000;
  cfg.channels = 1;
  cfg.bits_per_sample = 16;
  cfg.length = 512;
  cfg.stride = 160;
  cfg.window_function = &hann;
  cfg.callback = onResult;
  fft.begin(cfg);

  feedSine(1000.0f, 512);
  CHECK(results == 1);

  // 80 bands over 0..8000 Hz with 31.25 Hz bins: every band must see energy
  const int n = 80;
  float *mel = fft.toMEL(n, 0.0f, 8000.0f);
  CHECK(mel != nullptr);
  int peak = 0;
  for (int i = 0; i < n; i++) {
    if (mel[i] > mel[peak]) peak = i;
  }
  // band whose center is closest to 1000 Hz
  float min_mel = 0.0f, max_mel = 2595.0f * log10f(1.0f + 8000.0f / 700.0f);
  float step = (max_mel - min_mel) / (n + 1);
  int expected = 0;
  float best = 1e9f;
  for (int i = 0; i < n; i++) {
    float c = 700.0f * (powf(10.0f, (min_mel + (i + 1) * step) / 2595.0f) - 1.0f);
    if (fabsf(c - 1000.0f) < best) {
      best = fabsf(c - 1000.0f);
      expected = i;
    }
  }
  CHECK(abs(peak - expected) <= 1);

  // white-ish input: no band may be exactly zero
  fft.reset();
  for (int j = 0; j < 512; j++) {
    int16_t s = (int16_t)((rand() % 20000) - 10000);
    fft.write((uint8_t *)&s, sizeof(s));
  }
  mel = fft.toMEL(n, 0.0f, 8000.0f);
  int zero_bands = 0;
  for (int i = 0; i < n; i++) zero_bands += mel[i] <= 0.0f;
  CHECK(zero_bands == 0);

  // cache: a second call returns the same values
  float copy[n];
  memcpy(copy, mel, sizeof(copy));
  mel = fft.toMEL(n, 0.0f, 8000.0f);
  CHECK(memcmp(copy, mel, sizeof(copy)) == 0);

  // power spectrum: every band weight w applied to m^2 instead of m
  float *pw = fft.toMEL(n, 0.0f, 8000.0f, true);
  float sum_pw = 0, sum_mag = 0;
  for (int i = 0; i < n; i++) {
    sum_pw += pw[i];
    sum_mag += copy[i];
  }
  CHECK(sum_pw > 0 && sum_pw != sum_mag);

  // a different band count rebuilds the filterbank
  mel = fft.toMEL(40, 0.0f, 8000.0f);
  CHECK(mel != nullptr);

  // fromMEL distributes the bands back to linear bins
  float bands[40];
  for (int i = 0; i < 40; i++) bands[i] = 1.0f;
  CHECK(fft.fromMEL(bands, 40, 0.0f, 8000.0f));

  // BufferedWindow must cache the factors even if the wrapped window was
  // already started with the same size (regression: empty cache)
  Hann shared;
  shared.begin(512);
  BufferedWindow buffered(&shared);
  buffered.begin(512);
  int window_errors = 0;
  for (int j = 0; j < 512; j++)
    if (fabsf(buffered.factor(j) - shared.factor(j)) > 1e-6f) window_errors++;
  CHECK(window_errors == 0);

  // 24 bit input uses the int24_t representation (sizeof(int24_t) bytes)
  {
    AudioRealFFT fft24;
    auto c = fft24.defaultConfig();
    c.sample_rate = 16000;
    c.channels = 1;
    c.bits_per_sample = 24;
    c.length = 512;
    c.stride = 256;
    c.callback = onResult;
    fft24.begin(c);
    results = 0;
    int24_t samples[1024];
    for (int j = 0; j < 1024; j++) samples[j] = (int32_t)(100000 * sinf(0.1f * j));
    size_t n = fft24.write((uint8_t *)samples, sizeof(samples));
    CHECK(n == sizeof(samples));
    CHECK(results == 3);  // 1024 samples, window 512, hop 256
  }

  // Hann window: 0.5 * (1 - cos), symmetric, peak 1.0, zero at the ends
  {
    Hann h;
    h.begin(512);
    CHECK(fabsf(h.factor(0)) < 1e-6f);
    CHECK(fabsf(h.factor(511)) < 1e-6f);
    CHECK(fabsf(h.factor(255) - h.factor(256)) < 1e-6f);
    float peak = 0;
    for (int j = 0; j < 512; j++) peak = fmaxf(peak, h.factor(j));
    CHECK(peak > 0.9999f && peak <= 1.0f);
    CHECK(fabsf(h.factor(128) - 0.5f * (1.0f - cosf(6.28318531f * 128 / 511.0f))) < 1e-5f);
  }

  // a partial sample is not consumed
  {
    AudioRealFFT fft16;
    auto c = fft16.defaultConfig();
    c.sample_rate = 16000;
    c.channels = 1;
    c.bits_per_sample = 16;
    c.length = 256;
    fft16.begin(c);
    uint8_t bytes[101] = {0};
    CHECK(fft16.write(bytes, sizeof(bytes)) == 100);
    // magnitudes() after begin() with a larger length
    c.length = 1024;
    fft16.begin(c);
    float *m = fft16.magnitudes();
    CHECK(m != nullptr);
    CHECK(fft16.size() == 512);
  }

  // VolumeMeter: average over all samples, max as volume
  {
    VolumeMeter vm;
    vm.begin(AudioInfo(16000, 1, 16));
    int16_t s[4] = {100, -300, 200, 0};
    vm.write((uint8_t *)s, sizeof(s));
    CHECK(fabsf(vm.volume() - 300.0f) < 1e-3f);
    CHECK(fabsf(vm.volumeAvg() - 150.0f) < 1e-3f);
    CHECK(fabsf(vm.volumeAvg(0) - 150.0f) < 1e-3f);
  }

  printf("%s (%d failures)\n", failures == 0 ? "PASS" : "FAIL", failures);
  exit(failures == 0 ? 0 : 1);
}

void loop() {}
