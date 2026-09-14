/**
 * @file resample-performance.ino
 * @brief Measures the raw CPU cost of ResampleStream's interpolation math in
 * isolation, with NO adaptive control loop (fixed step_size) and no USB/I2S
 * involved at all - just a static in-memory buffer feeding the exact same
 * read path AdaptiveResamplingBuffer uses (TransformationReader pumping
 * ResampleStream::write()/writeFixed()).
 *
 * The source is a cheap precomputed static buffer (no floating-point sine
 * generation per sample) so the baseline reflects the real USB sketch,
 * where the source is just bytes already sitting in a ring buffer.
 *
 * For each (sample_rate, step_size) combination, it reads exactly 1 second
 * worth of resampled output as fast as possible and reports how long that
 * took. step_size = 1.0 is a no-op passthrough (baseline: pipe/plumbing
 * overhead only). step_size = 1.001 is representative of what the adaptive
 * resampler actually applies most of the time (small clock-drift
 * correction) - the delta between the two isolates the interpolation
 * math's true added cost.
 */
#include "AudioTools.h"

AudioInfo info(48000, 2, 16);
uint8_t src_buf[8192];
MemoryStream mem(src_buf, sizeof(src_buf), true);
ResampleStream resample(mem);
uint8_t out_buf[1024];

void runTest(int sample_rate, float step_size) {
  info.sample_rate = sample_rate;
  mem.begin(info);

  auto rcfg = resample.defaultConfig();
  rcfg.copyFrom(info);
  rcfg.step_size = step_size;
  resample.begin(rcfg);
  // Loop the small static source indefinitely instead of latching EOF once
  // it's drained once - same setting AdaptiveResamplingBuffer uses for a
  // live producer.
  resample.transformationReader().setEofOnZeroReads(false);
  resample.transformationReader().setZeroReadDelay(0);

  size_t bytes_per_sec =
      (size_t)sample_rate * info.channels * (info.bits_per_sample / 8);

  size_t total = 0;
  uint32_t start = micros();
  while (total < bytes_per_sec) {
    // Keep enough slack ahead of the internal chunk size so a read never
    // straddles the end of the buffer mid-chunk.
    if (mem.available() < 2048) mem.rewind();
    size_t to_read = min(sizeof(out_buf), bytes_per_sec - total);
    size_t got = resample.readBytes(out_buf, to_read);
    if (got == 0) break;
    total += got;
  }
  uint32_t elapsed_us = micros() - start;
  float cpu_percent = elapsed_us / 10000.0f;  // elapsed_us / 1e6 * 100

  Serial.print("sample_rate=");
  Serial.print(sample_rate);
  Serial.print("  step_size=");
  Serial.print(step_size, 4);
  Serial.print("  bytes=");
  Serial.print((unsigned long)total);
  Serial.print("  elapsed_us=");
  Serial.print(elapsed_us);
  Serial.print("  cpu_percent=");
  Serial.println(cpu_percent, 2);
}

void setup() {
  Serial.begin(115200);
  delay(3000);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // Cheap, non-constant, integer-only pattern - avoids floating point
  // entirely so we measure ONLY the resampling/plumbing cost, not source
  // generation cost.
  for (size_t i = 0; i < sizeof(src_buf); i += 2) {
    int16_t v = (int16_t)((i >> 2) & 0xFFF) - 2048;
    src_buf[i] = (uint8_t)(v & 0xFF);
    src_buf[i + 1] = (uint8_t)((v >> 8) & 0xFF);
  }

  Serial.println("--- ResampleStream performance test (static buffer, no sine) ---");
  runTest(32000, 1.0f);
  runTest(32000, 1.001f);
  runTest(44100, 1.0f);
  runTest(44100, 1.001f);
  runTest(48000, 1.0f);
  runTest(48000, 1.001f);
  Serial.println("--- done ---");
}

void loop() {}
