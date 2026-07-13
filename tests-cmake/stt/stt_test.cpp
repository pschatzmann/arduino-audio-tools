// Tests for AudioTools/AEC (LMSEchoCancellationStream, MDFEchoCancellationStream)
// and AudioTools/AudioLibs/WakeWordDetector.h.
//
// These are experimental ("Sandbox"-derived) components. This test suite intends to:
//  - lock in the structural fixes applied to get them to compile and run
//    (missing forward declarations, wrong constructor signatures, wrong
//    method names, wrong element types, wrong FFT driver type),
//  - verify the LMS canceller and the wake word detector are numerically and
//    functionally correct,
//  - verify the MDF canceller actually attenuates echo (see the comment on
//    test_mdf_converges for the chain of bugs -- two in the shared
//    AudioRealFFT.h driver, one here -- that had to be fixed for that to be
//    true; before those, it ran without crashing but never cancelled
//    anything).
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

#define IS_DESKTOP_WITH_TIME_ONLY 1
#include "AudioTools.h"
#include "AudioTools/FFT/AudioRealFFT.h"
#include "AudioTools/AEC/LMSEchoCancellationStream.h"
#include "AudioTools/AEC/MDFEchoCancellationStream.h"
#include "AudioTools/AudioLibs/WakeWordDetector.h"

using namespace audio_tools;

namespace {

double energy(const int16_t* data, size_t n) {
  double e = 0;
  for (size_t i = 0; i < n; i++) e += (double)data[i] * data[i];
  return e;
}

// ---------------------------------------------------------------------------
// LMSEchoCancellationStream
// ---------------------------------------------------------------------------

// Builds a synthetic "far-end" (speaker) signal and a "near-end" (mic) signal
// that contains that far-end signal passed through a small, fixed, 3-tap
// echo path (a stand-in for a real acoustic echo path) and nothing else, so
// full cancellation should push the residual error towards zero.
void makeLmsSignals(int16_t* play, int16_t* rec, int n) {
  for (int i = 0; i < n; i++) {
    play[i] = (int16_t)(4000 * sin(2 * M_PI * 300.0 * i / 8000.0));
  }
  for (int i = 0; i < n; i++) {
    float echo = 0.5f * play[i];
    if (i >= 3) echo += 0.25f * play[i - 3];
    if (i >= 7) echo += 0.1f * play[i - 7];
    rec[i] = (int16_t)echo;
  }
}

// Verifies the adaptive filter actually converges (residual echo energy
// drops substantially) and, since the run is far longer than the default
// ring buffer capacity (512 samples), also regression-tests the sliding
// delay-line fix: previously the ring buffer never evicted old samples, so
// it silently stopped adapting once it filled up.
void test_lms_converges_and_does_not_freeze() {
  const int n = 4000;
  static int16_t play[n], rec[n], out[n];
  makeLmsSignals(play, rec, n);

  MemoryStream dummy(nullptr, 0);
  LMSEchoCancellationStream<int16_t> lms(dummy, /*lag=*/0, /*buffer_size=*/512,
                                          /*filter_len=*/16, /*mu=*/0.01f);
  lms.cancel(rec, play, out, n);

  double e_first = energy(out, 200);
  double e_mid = energy(out + 2000, 200);  // well past the old 512-sample cap
  double e_last = energy(out + (n - 200), 200);
  double e_rec_first = energy(rec, 200);

  printf(
      "[lms] converge: e_rec_first=%.1f e_first=%.1f e_mid=%.1f e_last=%.1f\n",
      e_rec_first, e_first, e_mid, e_last);

  assert(e_first < e_rec_first);        // some cancellation from sample 0
  assert(e_mid < e_rec_first * 0.05);   // strongly converged well past 512
  assert(e_last < e_rec_first * 0.05);  // ... and stays converged (no freeze)
}

// Regression test for the write()/readBytes() plumbing: write() must feed
// the same delay line that readBytes() filters against. Verified by
// interleaving write()+readBytes() one sample at a time (mirroring how a
// full-duplex driver would call them) and checking it reproduces cancel()'s
// output exactly.
void test_lms_stream_pipeline_matches_direct_cancel() {
  const int n = 600;
  static int16_t play[n], rec[n], expected[n];
  makeLmsSignals(play, rec, n);

  MemoryStream unused(nullptr, 0);
  LMSEchoCancellationStream<int16_t> direct(unused, 2, 64, 16, 0.01f);
  direct.cancel(rec, play, expected, n);

  MemoryStream recStream((const uint8_t*)rec, sizeof(rec), true, FLASH_RAM);
  LMSEchoCancellationStream<int16_t> viaStream(recStream, 2, 64, 16, 0.01f);

  for (int i = 0; i < n; i++) {
    size_t wrote = viaStream.write((const uint8_t*)&play[i], sizeof(int16_t));
    assert(wrote == sizeof(int16_t));
    int16_t sample;
    size_t red = viaStream.readBytes((uint8_t*)&sample, sizeof(int16_t));
    assert(red == sizeof(int16_t));
    assert(sample == expected[i]);
  }
  printf("[lms] stream pipeline matches direct cancel() for all %d samples\n",
         n);
}

// ---------------------------------------------------------------------------
// WakeWordDetector
// ---------------------------------------------------------------------------

const char* g_wake_name = nullptr;
int g_wake_count = 0;

void onWakeWord(const char* name) {
  g_wake_name = name;
  g_wake_count++;
}

void fillTone(int16_t* buf, int n, float freq, float sample_rate, float amp) {
  for (int i = 0; i < n; i++) {
    buf[i] = (int16_t)(amp * sin(2 * M_PI * freq * i / sample_rate));
  }
}

// Verifies the detector recognizes a previously recorded tone and does not
// fire on an unrelated tone. Also doubles as a regression test for the
// ref/callback wiring bug: AudioFFTBase::begin(AudioFFTConfig) overwrites
// the whole config object, so setting fft.config().ref/callback only once
// (e.g. in the detector's constructor, before the caller's fft.begin(cfg))
// used to get silently wiped out. That's exactly the call order used here.
void test_wakeword_detects_known_tone_and_ignores_other() {
  const int len = 64;
  const float sample_rate = 8000;

  AudioRealFFT fft;
  WakeWordDetector<int16_t, 3> detector(fft);
  detector.setWakeWordCallback(onWakeWord);

  auto cfg = fft.defaultConfig(RXTX_MODE);
  cfg.length = len;
  cfg.channels = 1;
  cfg.bits_per_sample = 16;
  cfg.sample_rate = (int)sample_rate;
  assert(fft.begin(cfg));

  int16_t toneA[len], toneB[len];
  fillTone(toneA, len, 440.0f, sample_rate, 4000);   // the wake word
  fillTone(toneB, len, 1500.0f, sample_rate, 4000);  // an unrelated sound

  detector.startRecording();
  detector.write((const uint8_t*)toneA, sizeof(toneA));
  auto frames = detector.stopRecording();
  assert(frames.size() == 1);
  detector.addTemplate(frames, /*threshold_percent=*/50.0f, "hello");

  g_wake_count = 0;
  g_wake_name = nullptr;
  detector.write((const uint8_t*)toneA, sizeof(toneA));
  assert(g_wake_count == 1);
  assert(g_wake_name != nullptr && strcmp(g_wake_name, "hello") == 0);
  printf("[wakeword] matching tone triggered callback: %s\n", g_wake_name);

  g_wake_count = 0;
  g_wake_name = nullptr;
  detector.write((const uint8_t*)toneB, sizeof(toneB));
  assert(g_wake_count == 0);
  printf("[wakeword] unrelated tone correctly did not trigger callback\n");
}

// ---------------------------------------------------------------------------
// MDFEchoCancellationStream
// ---------------------------------------------------------------------------

// The structural bugs in this file (missing forward declarations that
// prevented compilation; a duplicate constructor signature; write()/
// readBytes() calling nonexistent/mismatched MDFEchoCancellation methods;
// echo_word16_t/echo_word32_t hardcoded to int16_t/int32_t despite the whole
// algorithm body using plain float arithmetic; fft_state storing the wrong
// FFT driver type; a Pey/Pyy division that computed 0/0 = NaN on the first
// frames) have all been fixed.
//
// Getting actual echo attenuation additionally required two fixes outside
// this file, in the shared AudioRealFFT.h driver:
//  - FFTDriverRealFFT::getBin()/magnitudeFast() read the real component
//    from v_x (do_fft()'s untouched time-domain input buffer) instead of
//    v_f (the actual packed FFT output) -- so every forward-FFT bin lookup
//    (used to build st->X/st->E/st->Y here) was wrong.
//  - FFTDriverRealFFT::begin(len) only (re)allocated its ffft::FFTReal
//    engine when none existed yet, so re-begin()ing an already-initialized
//    driver at a different length (exactly what echo_fft_init() does: the
//    MDF window is 2x the frame size the caller configured) silently kept
//    running the *old* size's transform on buffers resized for the new one.
// Plus one more bug local to this file: weightedSpectralMulConj() cast each
// frequency-domain sample through (int32_t) before multiplying -- a
// leftover from true fixed-point mode, where X/Y were int16_t and needed
// widening; in float mode it just truncated the filter's entire gradient
// signal to whole numbers (usually 0), so the adaptive filter never learned
// anything regardless of how correct the FFT was.
//
// With all of those fixed, this test asserts what actually matters: that a
// simple attenuated echo gets substantially cancelled.
void test_mdf_converges() {
  AudioRealFFT fft;
  auto cfg = fft.defaultConfig(RXTX_MODE);
  const int frame_size = 32;
  cfg.length = frame_size;
  cfg.channels = 1;
  cfg.bits_per_sample = 16;
  cfg.sample_rate = 8000;
  assert(fft.begin(cfg));

  const int num_frames = 300;
  const int spare_frames = 1;  // extra frame reserved for the post-reset check
  const int total = (num_frames + spare_frames) * frame_size;
  static int16_t play_all[(num_frames + spare_frames) * 32];
  static int16_t rec_all[(num_frames + spare_frames) * 32];
  for (int idx = 0; idx < total; idx++) {
    play_all[idx] = (int16_t)(5000 * sin(2 * M_PI * 440.0 * idx / 8000.0));
    rec_all[idx] = (int16_t)(0.7 * play_all[idx]);  // attenuated echo, no noise
  }

  MemoryStream recStream((const uint8_t*)rec_all, sizeof(rec_all), true,
                          FLASH_RAM);
  MDFEchoCancellationStream mdf(recStream, /*filterLength=*/96, fft);

  assert(mdf.getFilterLen() == 96);
  mdf.setFilterLen(64);
  assert(mdf.getFilterLen() == 64);
  mdf.setFilterLen(96);

  uint8_t buf[frame_size * sizeof(int16_t)];
  double e_first = 0, e_last = 0;
  for (int f = 0; f < num_frames; f++) {
    size_t wrote = mdf.write((const uint8_t*)&play_all[f * frame_size],
                              frame_size * sizeof(int16_t));
    assert(wrote == frame_size * sizeof(int16_t));

    size_t red = mdf.readBytes(buf, sizeof(buf));
    assert(red == sizeof(buf));

    int16_t* out = (int16_t*)buf;
    for (int i = 0; i < frame_size; i++) {
      assert(std::isfinite((float)out[i]));
    }
    double e = energy(out, frame_size);
    if (f < 10) e_first += e;
    if (f >= num_frames - 10) e_last += e;
  }

  printf("[mdf] e_first=%.1f e_last=%.1f ratio=%.6f\n", e_first, e_last,
         e_last / e_first);
  assert(e_last < e_first * 0.01);  // echo attenuated by at least 20dB

  auto* st = mdf.getEchoCanceller().getState();
  assert(st != nullptr);
  assert(std::isfinite((float)st->leak_estimate));
  assert(st->screwed_up < 50);  // never hit the internal instability reset
  printf("[mdf] converges: %d frames processed, no crash, no NaN\n",
         num_frames);

  mdf.reset();
  assert(st->adapted == 0);
  assert(st->screwed_up == 0);
  // must still be usable after reset()
  size_t wrote = mdf.write((const uint8_t*)&play_all[num_frames * frame_size],
                            frame_size * sizeof(int16_t));
  assert(wrote == frame_size * sizeof(int16_t));
  size_t red = mdf.readBytes(buf, sizeof(buf));
  assert(red == sizeof(buf));
  printf("[mdf] reset() clears state and remains usable afterwards\n");
}

}  // namespace

int main() {
  test_lms_converges_and_does_not_freeze();
  test_lms_stream_pipeline_matches_direct_cancel();
  test_wakeword_detects_known_tone_and_ignores_other();
  test_mdf_converges();
  printf("All AEC/wake-word sandbox tests passed.\n");
  return 0;
}
