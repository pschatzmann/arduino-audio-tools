// Functional parity check between the float and soft_float_t
// instantiations of AudioEffectsSuite.h's effect classes. Every class in
// that file is templated on the numeric type it uses internally
// (effectsuite_t), so both instantiations can be built side by side in this
// one binary and compared directly -- no separate PREFER_FIXEDPOINT=0/1
// builds needed.
//
// Effects are heap-allocated (matching how AudioEffects<>::addEffect()
// actually uses them), not stack locals -- stack-adjacent instances of
// these classes were observed to corrupt each other's state during
// development.
#include <cstdio>
#include <cstdlib>

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioEffectsSuite.h"

using namespace audio_tools;

int16_t arsineC256[] = { 436, 657, 877, 1096, 1316, 1534, 1751, 1967, 2183, 2396, 2609, 2819, 3029, 3236, 3441, 3644, 3845, 4044, 4240, 4434, 4625, 4813, 4999, 5181, 5360, 5536, 5709, 5878, 6043, 6205, 6364, 6518, 6668, 6815, 6957, 7095, 7229, 7359, 7484, 7604, 7720, 7831, 7938, 8040, 8137, 8229, 8316, 8398, 8475, 8547, 8613, 8675, 8731, 8782, 8828, 8868, 8903, 8933, 8957, 8976, 8989, 8997, 9000, 8997, 8989, 8975, 8956, 8932, 8902, 8867, 8826, 8780, 8729, 8672, 8610, 8544, 8471, 8394, 8312, 8225, 8133, 8035, 7933, 7827, 7715, 7599, 7478, 7353, 7223, 7089, 6951, 6808, 6662, 6511, 6357, 6198, 6036, 5870, 5701, 5528, 5352, 5173, 4990, 4805, 4617, 4425, 4232, 4035, 3836, 3635, 3432, 3227, 3019, 2810, 2599, 2387, 2173, 1958, 1742, 1524, 1306, 1087, 867, 647, 426, 205, -16, -237, -458, -679, -899, -1118, -1337, -1556, -1773, -1989, -2204, -2418, -2630, -2840, -3049, -3256, -3461, -3664, -3865, -4064, -4260, -4453, -4644, -4832, -5017, -5199, -5378, -5553, -5726, -5894, -6060, -6221, -6379, -6533, -6683, -6829, -6971, -7109, -7242, -7371, -7496, -7616, -7731, -7842, -7948, -8050, -8146, -8238, -8324, -8406, -8482, -8554, -8620, -8681, -8736, -8787, -8832, -8872, -8906, -8936, -8959, -8978, -8990, -8998, -9000, -8997, -8988, -8974, -8954, -8929, -8898, -8863, -8822, -8775, -8723, -8666, -8604, -8537, -8464, -8386, -8304, -8216, -8123, -8025, -7923, -7816, -7704, -7587, -7466, -7340, -7210, -7076, -6937, -6794, -6647, -6496, -6341, -6182, -6020, -5854, -5684, -5511, -5334, -5155, -4972, -4786, -4598, -4406, -4212, -4016, -3817, -3615, -3412, -3206, -2999, -2789, -2578, -2366, -2152, -1936, -1720, -1502, -1284, -1065, -845, -625, -404, -183, 38, 180 };
constexpr int N = 256;

// Pre-existing library quirk, unrelated to the fixed-point change: some
// EffectSuiteBase subclasses (e.g. SimpleFlanger) don't override process(),
// so they inherit it from EffectSuiteBase where it's accidentally private
// (no access specifier at the top of that class body, which defaults to
// private for `class`). Calling through an AudioEffect& sidesteps that,
// since access is checked against the static type used for the call.
//
// nSamples defaults to N but can be raised per-effect: SimpleChorus is a
// 100%-wet chorus voice (its `out = .0 * inputSample + 1. * ...` has no dry
// blend) with a ~661-882 sample delay range, so it reads only zeros from its
// still-empty delay buffer for its first ~835 samples -- comparing at N=256
// alone would just be comparing two silences and prove nothing.
void compare(const char* name, AudioEffect& floatEffect, AudioEffect& fixedEffect, int nSamples = N) {
  int maxDiff = 0;
  double sumDiff = 0;
  int floatMin = 32767, floatMax = -32768;
  for (int i = 0; i < nSamples; i++) {
    effect_t a = floatEffect.process(arsineC256[i % N]);
    effect_t b = fixedEffect.process(arsineC256[i % N]);
    int d = (int)a - (int)b;
    if (d < 0) d = -d;
    if (d > maxDiff) maxDiff = d;
    sumDiff += d;
    if (a < floatMin) floatMin = a;
    if (a > floatMax) floatMax = a;
  }
  printf("%-14s max_abs_diff=%-6d mean_abs_diff=%-9.3f float_range=(%d,%d)\n",
         name, maxDiff, sumDiff / nSamples, floatMin, floatMax);
}

// Proves clone() gives stereo-safe, independent state: `reference` and `a`
// are two freshly-constructed, identically-configured instances that never
// interact; `b` is a clone of `a` (the same relationship AudioEffectStreamT
// sets up per channel). `a` and `b` are fed *different* interleaved input,
// mimicking independent left/right channels. If clone() still shared
// buffers (the bug), b's different input would perturb a's internal state
// and its output would drift away from `reference`, which only ever saw
// the same input as `a`. Matching exactly proves no aliasing.
//
// nSamples defaults to N but needs raising for SimpleChorus (see compare()
// above): at N=256 it's comparing two silences, which would trivially
// "pass" even with aliased buffers since nothing has happened yet.
template <typename EffectType, typename... Args>
void testCloneIndependence(const char* name, int nSamples, Args&&... args) {
  EffectType* referenceT = new EffectType(args...);
  EffectType* aT = new EffectType(args...);
  // process() through AudioEffect&/*: some subclasses (e.g. SimpleFlanger)
  // don't override process(), so they inherit it from EffectSuiteBase where
  // it's accidentally private -- calling through the base sidesteps that,
  // since access is checked against the static type used for the call.
  AudioEffect& reference = *referenceT;
  AudioEffect& a = *aT;
  AudioEffect* b = aT->clone();

  int maxDiff = 0;
  for (int i = 0; i < nSamples; i++) {
    effect_t expected = reference.process(arsineC256[i % N]);
    effect_t actual = a.process(arsineC256[i % N]);
    b->process((effect_t)(-arsineC256[i % N]));  // different stream, interleaved
    int d = (int)expected - (int)actual;
    if (d < 0) d = -d;
    if (d > maxDiff) maxDiff = d;
  }
  printf("%-14s clone_independence_max_diff=%-4d %s\n", name, maxDiff,
         maxDiff == 0 ? "(OK: independent)" : "(FAIL: buffers aliased)");
}

void setup(void) {
  compare("SimpleLPF", *new SimpleLPF<float>(0.1f, 4),
          *new SimpleLPF<soft_float_t>(0.1f, 4));

  compare("SimpleChorus", *new SimpleChorus<float>(44100),
          *new SimpleChorus<soft_float_t>(44100), 3000);

  compare("SimpleDelay", *new SimpleDelay<float>(8810, 44100),
          *new SimpleDelay<soft_float_t>(8810, 44100));

  auto* flangerFloat = new SimpleFlanger<float>(44100.0f);
  flangerFloat->setEffectParams(0.7f, 44100 * 0.02f, 0.5f);
  auto* flangerFixed = new SimpleFlanger<soft_float_t>(44100.0f);
  flangerFixed->setEffectParams(0.7f, 44100 * 0.02f, 0.5f);
  compare("SimpleFlanger", *flangerFloat, *flangerFixed);

  compare("EnvelopeFilter", *new EnvelopeFilter<float>(),
          *new EnvelopeFilter<soft_float_t>());

  compare("FilteredDelay", *new FilteredDelay<float>(2205, 44100),
          *new FilteredDelay<soft_float_t>(2205, 44100));

  printf("\n");
  testCloneIndependence<SimpleLPF<float>>("SimpleLPF", N, 0.1f, 4);
  testCloneIndependence<SimpleChorus<float>>("SimpleChorus", 3000, 44100);
  testCloneIndependence<SimpleDelay<float>>("SimpleDelay", N, 8810, 44100);
  testCloneIndependence<SimpleFlanger<float>>("SimpleFlanger", N, 44100.0f);
  testCloneIndependence<EnvelopeFilter<float>>("EnvelopeFilter", N);
  testCloneIndependence<FilteredDelay<float>>("FilteredDelay", N, 2205, 44100);
}

void loop() { exit(0); }
