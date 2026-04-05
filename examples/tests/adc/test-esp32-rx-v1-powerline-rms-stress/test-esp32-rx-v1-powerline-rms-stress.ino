/**
 * @file test-esp32-rx-v1-powerline-rms-stress.ino
 * @brief FIFO top-off stress test for a 2-channel power-line RMS use case.
 *
 * Scenario:
 * - line frequency: 60 Hz
 * - channels: 2 (voltage and current)
 * - RMS window: 8 cycles = 133.33 ms
 * - target effective measurements after binning: 64 points/cycle
 *
 * For each bin size, the sketch increases the raw sample rate so the effective
 * post-bin rate stays at 64 points/cycle whenever the platform allows it:
 *
 * - bin 1  -> raw rate  3840 Hz/channel,   512 raw samples/window/channel
 * - bin 2  -> raw rate  7680 Hz/channel,  1024 raw samples/window/channel
 * - bin 4  -> raw rate 15360 Hz/channel,  2048 raw samples/window/channel
 * - bin 8  -> raw rate 30720 Hz/channel,  4096 raw samples/window/channel
 * - bin 16 -> raw rate 61440 Hz/channel,  8192 raw samples/window/channel
 *
 * The corresponding raw readBytes() window size is:
 *
 * - bin 1  ->  2048 bytes
 * - bin 2  ->  4096 bytes
 * - bin 4  ->  8192 bytes
 * - bin 8  -> 16384 bytes
 * - bin 16 -> 32768 bytes
 *
 * The larger windows are intentionally much larger than the default ADC RX pool
 * (4096 bytes with buffer_size=256 and buffer_count=4), so the driver must top
 * off the per-channel FIFOs multiple times during one call.
 *
 * On some 2-channel ESP32 targets the bin-16 rate may exceed the ADC
 * continuous sample-rate limit. In that case the sketch reports the scenario as
 * unsupported and skips it.
 *
 * Expected behavior:
 * - each supported window read returns raw_window_bytes / bin_size bytes
 * - short_reads remains 0
 * - no "FIFO buffer is full" messages appear
 */

#include "Arduino.h"
#include "AudioTools.h"
#include <math.h>

#if defined(ESP32) && defined(USE_ANALOG) && !USE_LEGACY_I2S

using namespace audio_tools;

AnalogDriverESP32V1 analog_driver;
AnalogAudioStream analog_in(analog_driver);
Bin binner;
ConverterStream<int16_t> binned_stream(analog_in, binner);

constexpr uint32_t kLineFrequencyHz = 60;
constexpr uint32_t kCyclesPerWindow = 8;
constexpr uint32_t kTargetPointsPerCycle = 64;
constexpr int kChannels = 2;
constexpr int kBitsPerSample = 16;
constexpr uint8_t kBinSizes[] = {1, 2, 4, 8, 16};
constexpr uint32_t kWindowsPerScenario = 8;
constexpr uint32_t kScenarioPauseMs = 250;
constexpr uint32_t kEffectiveSampleRateTarget =
    kLineFrequencyHz * kTargetPointsPerCycle;
constexpr uint32_t kMaxRawWindowSizeBytes =
    kEffectiveSampleRateTarget * 16U / kLineFrequencyHz * kCyclesPerWindow *
    kChannels * sizeof(int16_t);

uint8_t raw_buffer[kMaxRawWindowSizeBytes];

size_t bin_index = 0;
size_t first_supported_bin_index = 0;
bool cycle_initialized = false;
uint32_t windows_completed = 0;
uint32_t short_reads = 0;
size_t min_bytes_read = kMaxRawWindowSizeBytes;
size_t max_bytes_read = 0;
uint32_t current_raw_sample_rate = 0;
size_t current_raw_samples_per_cycle = 0;
size_t current_raw_samples_per_window_per_channel = 0;
size_t current_raw_window_size_bytes = 0;

size_t currentBinSize() { return kBinSizes[bin_index]; }

uint32_t desiredRawSampleRate() {
  return kEffectiveSampleRateTarget * currentBinSize();
}

uint32_t effectiveSampleRate() {
  if (currentBinSize() == 0) return 0;
  return current_raw_sample_rate / currentBinSize();
}

float effectivePointsPerCycle() {
  return (float)effectiveSampleRate() / (float)kLineFrequencyHz;
}

size_t expectedOutputBytes() {
  if (currentBinSize() == 0) return 0;
  return current_raw_window_size_bytes / currentBinSize();
}

double computeRms(const int16_t *samples, size_t frame_count, int channel) {
  if (frame_count == 0) return 0.0;

  double sum_sq = 0.0;
  for (size_t i = 0; i < frame_count; ++i) {
    double value = (double)samples[(i * kChannels) + channel];
    sum_sq += value * value;
  }

  return sqrt(sum_sq / (double)frame_count);
}

void resetScenarioStats() {
  windows_completed = 0;
  short_reads = 0;
  min_bytes_read = current_raw_window_size_bytes;
  max_bytes_read = 0;
}

void advanceScenario() {
  bin_index++;
  if (bin_index >= (sizeof(kBinSizes) / sizeof(kBinSizes[0]))) {
    bin_index = 0;
  }
}

bool applyScenario() {
  analog_in.end();

  current_raw_sample_rate = desiredRawSampleRate();
  current_raw_samples_per_cycle = current_raw_sample_rate / kLineFrequencyHz;
  current_raw_samples_per_window_per_channel =
      current_raw_samples_per_cycle * kCyclesPerWindow;
  current_raw_window_size_bytes =
      current_raw_samples_per_window_per_channel * kChannels *
      sizeof(int16_t);

  AnalogConfigESP32V1 cfg(RX_MODE);
  cfg.sample_rate = current_raw_sample_rate;
  cfg.channels = kChannels;
  cfg.bits_per_sample = kBitsPerSample;
  cfg.buffer_size = 256;
  cfg.buffer_count = 4;
  cfg.is_auto_center_read = true;
  cfg.adc_channels[0] = ADC_CHANNEL_4;
  cfg.adc_channels[1] = ADC_CHANNEL_5;

  binner.setChannels(cfg.channels);
  binner.setBits(cfg.bits_per_sample);
  binner.setAverage(true);
  binner.setBinSize((int)currentBinSize());
  binner.clear();

  Serial.println();
  Serial.println("ESP32 RX V1 power-line RMS FIFO stress test");
  Serial.println(
      "2-channel, 8-cycle RMS window with per-bin raw rate and buffer scaling");
  Serial.printf("line frequency: %lu Hz, RMS window: %lu cycles (%.2f ms)\n",
                (unsigned long)kLineFrequencyHz,
                (unsigned long)kCyclesPerWindow,
                1000.0f * (float)kCyclesPerWindow / (float)kLineFrequencyHz);
  Serial.printf(
      "raw sample rate: %lu Hz/channel, raw samples/window/channel: %u\n",
      (unsigned long)cfg.sample_rate,
      (unsigned)current_raw_samples_per_window_per_channel);
  Serial.printf(
      "bin: %u, effective sample rate: %lu Hz/channel, points/cycle: %.1f\n",
      (unsigned)currentBinSize(), (unsigned long)effectiveSampleRate(),
      effectivePointsPerCycle());
  Serial.printf("raw request: %u bytes, expected output: %u bytes\n",
                (unsigned)current_raw_window_size_bytes,
                (unsigned)expectedOutputBytes());

  if ((cfg.sample_rate * cfg.channels) > SOC_ADC_SAMPLE_FREQ_THRES_HIGH) {
    Serial.printf(
        "skipping: effective ADC rate %lu exceeds platform max %u\n",
        (unsigned long)(cfg.sample_rate * cfg.channels),
        (unsigned)SOC_ADC_SAMPLE_FREQ_THRES_HIGH);
    delay(kScenarioPauseMs);
    return false;
  }

  if (!analog_in.begin(cfg)) {
    Serial.println("analog_in.begin failed");
    return false;
  }

  delay(50);
  resetScenarioStats();
  return true;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  while (!applyScenario()) {
    advanceScenario();
    if (bin_index == 0) {
      Serial.println("no supported scenarios found");
      while (true) {
        delay(1000);
      }
    }
  }

  first_supported_bin_index = bin_index;
  cycle_initialized = true;
}

void loop() {
  int avail_before = analog_in.available();
  size_t bytes_read =
      binned_stream.readBytes(raw_buffer, current_raw_window_size_bytes);
  int avail_after = analog_in.available();

  windows_completed++;
  if (bytes_read != expectedOutputBytes()) {
    short_reads++;
  }
  if (bytes_read < min_bytes_read) min_bytes_read = bytes_read;
  if (bytes_read > max_bytes_read) max_bytes_read = bytes_read;

  size_t frame_count = bytes_read / (kChannels * sizeof(int16_t));
  int16_t *samples = (int16_t *)raw_buffer;
  double voltage_rms = computeRms(samples, frame_count, 0);
  double current_rms = computeRms(samples, frame_count, 1);

  Serial.printf(
      "bin=%u window=%lu/%lu bytes=%u expected=%u short_reads=%lu "
      "avail_before=%d avail_after=%d Vrms=%.2f Irms=%.2f\n",
      (unsigned)currentBinSize(), (unsigned long)windows_completed,
      (unsigned long)kWindowsPerScenario, (unsigned)bytes_read,
      (unsigned)expectedOutputBytes(), (unsigned long)short_reads,
      avail_before, avail_after, voltage_rms, current_rms);

  if (windows_completed >= kWindowsPerScenario) {
    Serial.printf(
        "summary bin=%u bytes[min/max]=%u/%u short_reads=%lu "
        "points/cycle=%.1f\n",
        (unsigned)currentBinSize(), (unsigned)min_bytes_read,
        (unsigned)max_bytes_read, (unsigned long)short_reads,
        effectivePointsPerCycle());

    delay(kScenarioPauseMs);
    do {
      advanceScenario();
      if (bin_index == 0) {
        Serial.println("completed all supported scenarios");
        while (true) {
          delay(1000);
        }
      }
      if (cycle_initialized && bin_index == first_supported_bin_index) {
        analog_in.end();
        Serial.println("completed one full supported scenario cycle");
        while (true) {
          delay(1000);
        }
      }
      if (applyScenario()) break;
    } while (true);
  }
}

#else

void setup() {}
void loop() {}

#endif
