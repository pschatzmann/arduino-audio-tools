/**
 * @file test-esp32-rx-v1-acceptance.ino
 * @brief Acceptance test for the ESP32 ADC v1 FIFO reorder redesign.
 *
 * This sketch verifies the key behaviors introduced by the redesign:
 * - large multichannel readBytes() windows complete across multiple ADC chunks
 * - no request-sized temporary allocation is performed inside readBytes()
 * - available() is driven by already formable frames when buffered data exists
 * - auto-centering still runs on the completed returned block
 *
 * Suggested wiring for stronger validation:
 * - connect ADC_CHANNEL_4 and ADC_CHANNEL_5 to the same analog source
 *   (for example the same signal generator output through the same divider)
 * - with both channels driven from the same source, channel delta statistics
 *   should stay small and stable
 *
 * Expected pass criteria:
 * - short_reads remains 0 for every request-size scenario
 * - heap deltas remain near zero while request sizes change
 * - available_before eventually reports non-zero values that are bounded to
 *   assembled frame readiness instead of scaling with the full request window
 * - if both channels are tied together, avg_abs_delta remains small
 */

#include "Arduino.h"
#include "AudioTools.h"

#if defined(ESP32) && defined(USE_ANALOG) && !USE_LEGACY_I2S

#include "esp_heap_caps.h"

using namespace audio_tools;

AnalogDriverESP32V1 analog_driver;
AnalogAudioStream analog_in(analog_driver);

constexpr int kChannels = 2;
constexpr int kBitsPerSample = 16;
constexpr uint32_t kSampleRate = 24000;
constexpr uint16_t kBufferSize = 256;
constexpr uint8_t kBufferCount = 4;
constexpr uint32_t kScenarioPauseMs = 250;
constexpr uint32_t kWarmupDelayMs = 60;
constexpr uint32_t kWindowsPerScenario = 12;
constexpr size_t kRequestSizes[] = {256, 1024, 4096, 8192, 16384};
constexpr size_t kMaxRequestSize = 16384;

uint8_t sample_buffer[kMaxRequestSize];

size_t scenario_index = 0;
bool all_scenarios_completed = false;
uint32_t windows_completed = 0;
uint32_t short_reads = 0;
size_t min_bytes_read = kMaxRequestSize;
size_t max_bytes_read = 0;
int min_avail_before = 0x7fffffff;
int max_avail_before = 0;
int min_avail_after = 0x7fffffff;
int max_avail_after = 0;
uint32_t baseline_free_heap = 0;
uint32_t min_free_heap = 0xffffffffu;
uint32_t max_free_heap = 0;
uint32_t min_heap_delta = 0xffffffffu;
uint32_t max_heap_delta = 0;
double min_block_mean_ch0 = 1e9;
double max_block_mean_ch0 = -1e9;
double min_block_mean_ch1 = 1e9;
double max_block_mean_ch1 = -1e9;
double min_avg_abs_delta = 1e9;
double max_avg_abs_delta = 0.0;

size_t currentRequestSize() { return kRequestSizes[scenario_index]; }

uint32_t currentFreeHeap() {
  return heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

void resetScenarioStats() {
  windows_completed = 0;
  short_reads = 0;
  min_bytes_read = currentRequestSize();
  max_bytes_read = 0;
  min_avail_before = 0x7fffffff;
  max_avail_before = 0;
  min_avail_after = 0x7fffffff;
  max_avail_after = 0;
  baseline_free_heap = currentFreeHeap();
  min_free_heap = baseline_free_heap;
  max_free_heap = baseline_free_heap;
  min_heap_delta = 0;
  max_heap_delta = 0;
  min_block_mean_ch0 = 1e9;
  max_block_mean_ch0 = -1e9;
  min_block_mean_ch1 = 1e9;
  max_block_mean_ch1 = -1e9;
  min_avg_abs_delta = 1e9;
  max_avg_abs_delta = 0.0;
}

void advanceScenario() {
  scenario_index++;
  if (scenario_index >= (sizeof(kRequestSizes) / sizeof(kRequestSizes[0]))) {
    all_scenarios_completed = true;
  }
}

void updateBlockStats(const int16_t *samples, size_t frame_count, double &mean0,
                      double &mean1, double &avg_abs_delta) {
  mean0 = 0.0;
  mean1 = 0.0;
  avg_abs_delta = 0.0;
  if (frame_count == 0) return;

  double sum0 = 0.0;
  double sum1 = 0.0;
  double delta_sum = 0.0;
  for (size_t i = 0; i < frame_count; ++i) {
    int16_t ch0 = samples[(i * kChannels) + 0];
    int16_t ch1 = samples[(i * kChannels) + 1];
    sum0 += (double)ch0;
    sum1 += (double)ch1;
    delta_sum += abs((int)ch0 - (int)ch1);
  }

  mean0 = sum0 / (double)frame_count;
  mean1 = sum1 / (double)frame_count;
  avg_abs_delta = delta_sum / (double)frame_count;

  if (mean0 < min_block_mean_ch0) min_block_mean_ch0 = mean0;
  if (mean0 > max_block_mean_ch0) max_block_mean_ch0 = mean0;
  if (mean1 < min_block_mean_ch1) min_block_mean_ch1 = mean1;
  if (mean1 > max_block_mean_ch1) max_block_mean_ch1 = mean1;
  if (avg_abs_delta < min_avg_abs_delta) min_avg_abs_delta = avg_abs_delta;
  if (avg_abs_delta > max_avg_abs_delta) max_avg_abs_delta = avg_abs_delta;
}

bool applyScenario() {
  analog_in.end();

  auto cfg = analog_in.defaultConfig(RX_MODE);
  cfg.sample_rate = kSampleRate;
  cfg.channels = kChannels;
  cfg.bits_per_sample = kBitsPerSample;
  cfg.buffer_size = kBufferSize;
  cfg.buffer_count = kBufferCount;
  cfg.is_auto_center_read = true;
  cfg.adc_channels[0] = ADC_CHANNEL_4;
  cfg.adc_channels[1] = ADC_CHANNEL_5;

  Serial.println();
  Serial.println("ESP32 RX V1 acceptance test");
  Serial.println("Large multichannel raw reads with heap and availability checks");
  Serial.printf("sample_rate: %lu Hz/channel, channels: %d\n",
                (unsigned long)cfg.sample_rate, cfg.channels);
  Serial.printf("buffer_size: %u results, buffer_count: %u\n",
                (unsigned)cfg.buffer_size, (unsigned)cfg.buffer_count);
  Serial.printf("request_size: %u bytes\n", (unsigned)currentRequestSize());
  Serial.println("optional: tie ADC_CHANNEL_4 and ADC_CHANNEL_5 together");

  if (!analog_in.begin(cfg)) {
    Serial.println("analog_in.begin failed");
    return false;
  }

  delay(kWarmupDelayMs);
  resetScenarioStats();
  return true;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  if (!applyScenario()) {
    while (true) {
      delay(1000);
    }
  }
}

void loop() {
  if (all_scenarios_completed) {
    delay(1000);
    return;
  }

  int avail_before = analog_in.available();
  size_t bytes_read = analog_in.readBytes(sample_buffer, currentRequestSize());
  int avail_after = analog_in.available();
  uint32_t free_heap = currentFreeHeap();
  uint32_t heap_delta =
      (baseline_free_heap > free_heap) ? (baseline_free_heap - free_heap)
                                       : (free_heap - baseline_free_heap);

  windows_completed++;
  if (bytes_read != currentRequestSize()) {
    short_reads++;
  }
  if (bytes_read < min_bytes_read) min_bytes_read = bytes_read;
  if (bytes_read > max_bytes_read) max_bytes_read = bytes_read;
  if (avail_before < min_avail_before) min_avail_before = avail_before;
  if (avail_before > max_avail_before) max_avail_before = avail_before;
  if (avail_after < min_avail_after) min_avail_after = avail_after;
  if (avail_after > max_avail_after) max_avail_after = avail_after;
  if (free_heap < min_free_heap) min_free_heap = free_heap;
  if (free_heap > max_free_heap) max_free_heap = free_heap;
  if (heap_delta < min_heap_delta) min_heap_delta = heap_delta;
  if (heap_delta > max_heap_delta) max_heap_delta = heap_delta;

  size_t frame_count = bytes_read / (kChannels * sizeof(int16_t));
  double current_mean0 = 0.0;
  double current_mean1 = 0.0;
  double current_avg_abs_delta = 0.0;
  updateBlockStats((const int16_t *)sample_buffer, frame_count, current_mean0,
                   current_mean1, current_avg_abs_delta);

  Serial.printf(
      "req=%u window=%lu/%lu bytes=%u short_reads=%lu "
      "avail_before=%d avail_after=%d heap=%lu delta=%lu "
      "mean0=%.1f mean1=%.1f avg_abs_delta=%.1f\n",
      (unsigned)currentRequestSize(), (unsigned long)windows_completed,
      (unsigned long)kWindowsPerScenario, (unsigned)bytes_read,
      (unsigned long)short_reads, avail_before, avail_after,
      (unsigned long)free_heap, (unsigned long)heap_delta, current_mean0,
      current_mean1, current_avg_abs_delta);

  if (windows_completed >= kWindowsPerScenario) {
    Serial.printf(
        "summary req=%u bytes[min/max]=%u/%u short_reads=%lu "
        "avail_before[min/max]=%d/%d avail_after[min/max]=%d/%d "
        "heap[min/max]=%lu/%lu delta[min/max]=%lu/%lu "
        "mean0[min/max]=%.1f/%.1f mean1[min/max]=%.1f/%.1f "
        "avg_abs_delta[min/max]=%.1f/%.1f\n",
        (unsigned)currentRequestSize(), (unsigned)min_bytes_read,
        (unsigned)max_bytes_read, (unsigned long)short_reads,
        min_avail_before, max_avail_before, min_avail_after, max_avail_after,
        (unsigned long)min_free_heap, (unsigned long)max_free_heap,
        (unsigned long)min_heap_delta, (unsigned long)max_heap_delta,
        min_block_mean_ch0, max_block_mean_ch0, min_block_mean_ch1,
        max_block_mean_ch1, min_avg_abs_delta, max_avg_abs_delta);

    delay(kScenarioPauseMs);
    advanceScenario();
    if (all_scenarios_completed) {
      analog_in.end();
      Serial.println("acceptance test completed all request-size scenarios");
      while (true) {
        delay(1000);
      }
    }
    if (!applyScenario()) {
      while (true) {
        delay(1000);
      }
    }
  }
}

#else

void setup() {}
void loop() {}

#endif
