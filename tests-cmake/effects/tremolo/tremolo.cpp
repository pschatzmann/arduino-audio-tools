// Tremolo (AudioEffect.h): rate and depth of both waveforms, smooth gain,
// auto pan (phase), click free parameter changes, full scale input. Returns
// the number of failed checks.
#include <cmath>
#include <cstdio>
#include <vector>
#include "AudioTools.h"
#include "AudioTools/CoreAudio/AudioEffects/AudioEffect.h"
using namespace audio_tools;
static const int RATE = 44100;
static int failures = 0;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAILED: " __VA_ARGS__); printf("\n"); } } while (0)

// gain per sample: constant input
template <class T> std::vector<float> gains(T& t, int n) {
  std::vector<float> g(n);
  for (int i = 0; i < n; i++) g[i] = t.process(16384) / 16384.0f;
  return g;
}
// rate from the minima of the gain
float measureRate(const std::vector<float>& g) {
  int crossings = 0; float mean = 0;
  for (float v : g) mean += v;
  mean /= g.size();
  for (size_t i = 1; i < g.size(); i++) if ((g[i - 1] - mean) < 0 && (g[i] - mean) >= 0) crossings++;
  return crossings / (g.size() / (float)RATE);
}

void setup() {
  // 2. rate and depth of both waveforms
  for (auto shape : {Tremolo::Triangle, Tremolo::Sine}) {
    Tremolo t(2000, 40, RATE);
    t.setShape(shape);
    t.setRate(5.5f);
    auto g = gains(t, RATE * 4);
    float lo = 2, hi = 0;
    for (float v : g) { lo = std::min(lo, v); hi = std::max(hi, v); }
    float rate = measureRate(g);
    printf("%s: rate %.2f Hz (5.5), gain %.3f .. %.3f (0.6 .. 1.0)\n", shape == Tremolo::Sine ? "sine" : "triangle", rate, lo, hi);
    CHECK(fabs(rate - 5.5f) < 0.3f, "rate");
    CHECK(fabs(lo - 0.6f) < 0.01f && hi <= 1.0f && hi > 0.99f, "depth");
    // smooth: max gain change per sample (sine 5.5 Hz, depth 0.4: max slope 0.4*pi*5.5/44100 = 0.00016)
    float maxstep = 0;
    for (size_t i = 1; i < g.size(); i++) maxstep = std::max(maxstep, fabsf(g[i] - g[i - 1]));
    printf("   max gain change per sample %.5f\n", maxstep);
    CHECK(maxstep < 0.0005f, "smooth gain");
  }
  // 3. auto pan: 180 degrees = opposite phase
  {
    Tremolo l(2000, 80, RATE), r(2000, 80, RATE);
    l.setShape(Tremolo::Sine); r.setShape(Tremolo::Sine);
    l.setRate(4.0f); r.setRate(4.0f); r.setPhase(180);
    auto gl = gains(l, RATE), gr = gains(r, RATE);
    double corr = 0, ml = 0, mr = 0;
    for (int i = 0; i < RATE; i++) { ml += gl[i]; mr += gr[i]; }
    ml /= RATE; mr /= RATE;
    double sl = 0, sr = 0;
    for (int i = 0; i < RATE; i++) { corr += (gl[i] - ml) * (gr[i] - mr); sl += (gl[i] - ml) * (gl[i] - ml); sr += (gr[i] - mr) * (gr[i] - mr); }
    corr /= sqrt(sl * sr);
    printf("auto pan: correlation left / right %.3f (-1)\n", corr);
    CHECK(corr < -0.99, "opposite phase");
  }
  // 4. parameter changes while playing: no jumps
  {
    Tremolo t(200, 30, RATE);
    t.setShape(Tremolo::Sine);
    float last = t.process(16384) / 16384.0f, maxstep = 0;
    for (int i = 0; i < RATE; i++) {
      if (i == 10000) t.setDepth(90);
      if (i == 20000) t.setRate(8.0f);
      if (i == 30000) t.setDepth(0);
      float g = t.process(16384) / 16384.0f;
      maxstep = std::max(maxstep, fabsf(g - last));
      last = g;
    }
    printf("parameter changes: max gain change per sample %.4f\n", maxstep);
    CHECK(maxstep < 0.03f, "click free parameter changes");
    CHECK(fabsf(last - 1.0f) < 0.001f, "depth 0 = unchanged");
  }
  // 5. full scale input: no overflow
  {
    Tremolo t(100, 100, RATE);
    int16_t mn = 0, mx = 0;
    for (int i = 0; i < RATE; i++) { int16_t v = t.process(i % 2 ? 32767 : -32768); mn = std::min(mn, v); mx = std::max(mx, v); }
    CHECK(mx <= 32767 && mn >= -32768 && mx > 0, "full scale");
  }
  printf("%s\n", failures ? "FAILED" : "all checks passed");
  exit(failures);
}

void loop() {}
