// Verifies that Equalizer3BandsT<T>/Equalizer3BandsPerChannelT<T> work
// correctly for T=float (the original behavior), T=q1_14_t and
// T=soft_float_t (the FPU-less fixed-point/software-float variants added to
// let RP2040 etc. avoid native float instructions), and that the
// backwards-compatible Equalizer3Bands/Equalizer3BandsPerChannel aliases
// (float-based, no template argument) still work unchanged.
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

#include "AudioTools.h"
#include "AudioTools/CoreAudio/AudioBasic/soft_float_t.h"
#include "AudioTools/CoreAudio/AudioFilter.h"

using namespace audio_tools;

namespace {

class NullPrint : public Print {
 public:
  size_t write(uint8_t) override { return 1; }
  size_t write(const uint8_t* data, size_t len) override { return len; }
};

double rms(const int16_t* data, size_t n) {
  double sumsq = 0;
  for (size_t i = 0; i < n; i++) sumsq += (double)data[i] * data[i];
  return sqrt(sumsq / n);
}

// Runs a stereo sine tone through the equalizer and returns the RMS of
// channel 0's output.
template <typename T>
double runTone(double freq, int freq_low, int freq_high, float gain_low,
               float gain_medium, float gain_high, int n = 4000) {
  NullPrint out;
  Equalizer3BandsT<T> eq(out);
  auto cfg = eq.defaultConfig();
  cfg.channels = 2;
  cfg.bits_per_sample = 16;
  cfg.sample_rate = 44100;
  cfg.freq_low = freq_low;
  cfg.freq_high = freq_high;
  cfg.gain_low = gain_low;
  cfg.gain_medium = gain_medium;
  cfg.gain_high = gain_high;
  assert(eq.begin(cfg));

  std::vector<int16_t> ch0(n);
  int16_t buf[2];
  for (int i = 0; i < n; i++) {
    int16_t s = (int16_t)(10000.0 * sin(2 * M_PI * freq * i / 44100.0));
    buf[0] = s;
    buf[1] = s;
    eq.write((uint8_t*)buf, sizeof(buf));
    ch0[i] = buf[0];
  }
  // skip the filter's startup transient (state starts at zero)
  return rms(ch0.data() + 200, n - 200);
}

// With all gains at unity, the 3-band split (low + mid + high) reconstructs
// the original signal, for any arithmetic type T -- this is a property of
// the filter design, not of T, so it should hold for float, q1_14_t and
// soft_float_t alike (within each type's own precision).
template <typename T>
void test_identity_gain_reconstructs_signal(const char* label,
                                             double tol_ratio) {
  double in_rms = 10000.0 / sqrt(2.0);  // RMS of a 10000-amplitude sine
  double out_rms = runTone<T>(440.0, 880, 5000, 1.0f, 1.0f, 1.0f);
  double diff_ratio = fabs(out_rms - in_rms) / in_rms;
  printf("[%s] identity gain: in_rms=%.1f out_rms=%.1f diff_ratio=%.4f\n",
         label, in_rms, out_rms, diff_ratio);
  assert(diff_ratio < tol_ratio);
}

// Cutting a band's gain to 0 should noticeably reduce the RMS of a tone deep
// inside that band; boosting it should noticeably increase it. Frequencies
// are chosen well away from the freq_low/freq_high crossover points -- close
// to a crossover the bands overlap and interact in phase-dependent ways, so
// cutting one band's gain there does not move total RMS monotonically. This
// is a functional check that band gains actually take effect for T.
template <typename T>
void test_gain_changes_energy(const char* label) {
  // 100Hz is deep in the low band (freq_low=880)
  double unity_low = runTone<T>(100.0, 880, 5000, 1.0f, 1.0f, 1.0f);
  double cut_low = runTone<T>(100.0, 880, 5000, 0.0f, 1.0f, 1.0f);
  double boost_low = runTone<T>(100.0, 880, 5000, 2.0f, 1.0f, 1.0f);
  printf("[%s] low-band gain sweep: cut=%.1f unity=%.1f boost=%.1f\n", label,
         cut_low, unity_low, boost_low);
  assert(cut_low < unity_low);
  assert(boost_low > unity_low);

  // 8000Hz is deep in the high band (freq_high=5000)
  double unity_high = runTone<T>(8000.0, 880, 5000, 1.0f, 1.0f, 1.0f);
  double cut_high = runTone<T>(8000.0, 880, 5000, 1.0f, 1.0f, 0.0f);
  double boost_high = runTone<T>(8000.0, 880, 5000, 1.0f, 1.0f, 2.0f);
  printf("[%s] high-band gain sweep: cut=%.1f unity=%.1f boost=%.1f\n", label,
         cut_high, unity_high, boost_high);
  assert(cut_high < unity_high);
  assert(boost_high > unity_high);
}

// q1_14_t/soft_float_t should track the float reference implementation
// closely for the same configuration -- confirms the fixed-point/soft-float
// variants aren't just "compiling", but computing the (approximately) same
// filter as float.
template <typename T>
void test_matches_float_reference(const char* label, double tol_ratio) {
  double float_rms = runTone<float>(440.0, 880, 5000, 1.5f, 0.8f, 1.2f);
  double t_rms = runTone<T>(440.0, 880, 5000, 1.5f, 0.8f, 1.2f);
  double diff_ratio = fabs(t_rms - float_rms) / float_rms;
  printf("[%s] vs float: float_rms=%.1f %s_rms=%.1f diff_ratio=%.4f\n", label,
         float_rms, label, t_rms, diff_ratio);
  assert(diff_ratio < tol_ratio);
}

// Per-channel variant: independent frequencies/gains per channel should
// produce different output for the same input on each channel.
template <typename T>
void test_per_channel_independence(const char* label) {
  NullPrint out;
  Equalizer3BandsPerChannelT<T> eq(out);
  auto cfg = eq.defaultConfig();
  cfg.channels = 2;
  cfg.bits_per_sample = 16;
  cfg.sample_rate = 44100;
  assert(eq.begin(cfg));
  eq.setChannelGains(0, 2.0f, 1.0f, 1.0f);   // boost low on ch 0
  eq.setChannelGains(1, 0.0f, 1.0f, 1.0f);   // cut low on ch 1
  eq.setChannelFrequencies(0, 880, 5000);
  eq.setChannelFrequencies(1, 880, 5000);

  double sumsq0 = 0, sumsq1 = 0;
  const int n = 2000;
  for (int i = 0; i < n; i++) {
    int16_t s = (int16_t)(10000.0 * sin(2 * M_PI * 220.0 * i / 44100.0));
    int16_t buf[2] = {s, s};
    eq.write((uint8_t*)buf, sizeof(buf));
    if (i >= 200) {
      sumsq0 += (double)buf[0] * buf[0];
      sumsq1 += (double)buf[1] * buf[1];
    }
  }
  double rms0 = sqrt(sumsq0 / (n - 200));
  double rms1 = sqrt(sumsq1 / (n - 200));
  printf("[%s] per-channel: rms_boosted=%.1f rms_cut=%.1f\n", label, rms0,
         rms1);
  assert(rms0 > rms1);
}

// The backwards-compatible aliases (no template argument) must keep working
// exactly as before the class was templated.
void test_backward_compat_aliases() {
  NullPrint out;
  Equalizer3Bands eq(out);
  auto cfg = eq.defaultConfig();
  cfg.channels = 2;
  assert(eq.begin(cfg));
  int16_t buf[2] = {1000, -1000};
  eq.write((uint8_t*)buf, sizeof(buf));

  NullPrint out2;
  Equalizer3BandsPerChannel eqpc(out2);
  auto cfg2 = eqpc.defaultConfig();
  cfg2.channels = 2;
  assert(eqpc.begin(cfg2));
  eqpc.setChannelGains(0, 1.2f, 1.0f, 0.9f);
  eqpc.write((uint8_t*)buf, sizeof(buf));
  printf("[compat] Equalizer3Bands / Equalizer3BandsPerChannel aliases OK\n");
}

}  // namespace

int main() {
  test_backward_compat_aliases();

  test_identity_gain_reconstructs_signal<float>("float", 0.02);
  test_identity_gain_reconstructs_signal<q1_14_t>("q1_14_t", 0.05);
  test_identity_gain_reconstructs_signal<soft_float_t>("soft_float_t", 0.02);

  test_gain_changes_energy<float>("float");
  test_gain_changes_energy<q1_14_t>("q1_14_t");
  test_gain_changes_energy<soft_float_t>("soft_float_t");

  test_matches_float_reference<q1_14_t>("q1_14_t", 0.02);
  test_matches_float_reference<soft_float_t>("soft_float_t", 0.02);

  test_per_channel_independence<float>("float");
  test_per_channel_independence<q1_14_t>("q1_14_t");
  test_per_channel_independence<soft_float_t>("soft_float_t");

  printf("All equalizer template tests passed.\n");
  return 0;
}
