/**
 * @file test-esp32-rx-v1-bin-oversampling.ino
 * @brief Regression test for the ESP32 ADC v1 FIFO sizing fix.
 *
 * This sketch exercises the failure mode where AnalogDriverESP32V1 receives a
 * single large raw read request through ConverterStream<Bin>. Before the fix,
 * the driver sized its per-channel FIFO from one DMA frame only, so larger
 * reads could overflow the FIFO while the driver was topping off additional ADC
 * data.
 *
 * What this test keeps constant:
 * - AnalogDriverESP32V1 in RX mode
 * - 2 ADC channels
 * - 16-bit samples
 * - one raw-block read request of 2048 bytes on every loop iteration
 * - auto-centering enabled
 * - ADC DMA frame configuration: buffer_size=256 results, buffer_count=4
 *
 * What this test changes while running:
 * - sample_rate: 8000, 16000, 32000 Hz
 * - bin size: 1, 2, 4, 8, 16, 32
 *
 * Expected behavior:
 * - the fixed 2048-byte raw read request should complete without FIFO overflow
 * - the returned byte count should be 2048 / bin_size
 * - short_reads should remain at 0 during each scenario
 */

#include "Arduino.h"
#include "AudioTools.h"

#if defined(ESP32) && defined(USE_ANALOG) && !USE_LEGACY_I2S

using namespace audio_tools;

AnalogDriverESP32V1 analog_driver;
AnalogAudioStream analog_in(analog_driver);
Bin binner;
ConverterStream<int16_t> binned_stream(analog_in, binner);

constexpr size_t kRawBufferSizeBytes = 2048;
constexpr uint32_t kScenarioDurationMs = 5000;
constexpr uint32_t kReportIntervalMs = 1000;
constexpr uint32_t kSampleRates[] = {8000, 16000, 32000};
constexpr uint8_t kBinSizes[] = {1, 2, 4, 8, 16, 32};
uint8_t raw_buffer[kRawBufferSizeBytes];

size_t sample_rate_index = 0;
size_t bin_size_index = 0;
uint32_t last_report_ms = 0;
uint32_t scenario_started_ms = 0;
uint32_t iterations = 0;
uint32_t short_reads = 0;
size_t min_bytes_read = kRawBufferSizeBytes;
size_t max_bytes_read = 0;
int min_avail_before = 0x7fffffff;
int max_avail_before = 0;
int min_avail_after = 0x7fffffff;
int max_avail_after = 0;

void resetStats() {
  iterations = 0;
  short_reads = 0;
  min_bytes_read = kRawBufferSizeBytes;
  max_bytes_read = 0;
  min_avail_before = 0x7fffffff;
  max_avail_before = 0;
  min_avail_after = 0x7fffffff;
  max_avail_after = 0;
}

size_t currentBinSize() { return kBinSizes[bin_size_index]; }

uint32_t currentSampleRate() { return kSampleRates[sample_rate_index]; }

size_t expectedOutputBytes() { return kRawBufferSizeBytes / currentBinSize(); }

void advanceScenario() {
  bin_size_index++;
  if (bin_size_index >= (sizeof(kBinSizes) / sizeof(kBinSizes[0]))) {
    bin_size_index = 0;
    sample_rate_index++;
    if (sample_rate_index >= (sizeof(kSampleRates) / sizeof(kSampleRates[0]))) {
      sample_rate_index = 0;
    }
  }
}

bool applyScenario() {
  analog_in.end();

  AnalogConfigESP32V1 cfg(RX_MODE);
  cfg.sample_rate = currentSampleRate();
  cfg.channels = 2;
  cfg.bits_per_sample = 16;
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
  Serial.println("ESP32 RX V1 bin oversampling test");
  Serial.println(
      "Using AnalogDriverESP32V1 with ConverterStream<Bin> and one raw-block "
      "read per call");
  Serial.println("This test keeps raw readBytes() fixed and sweeps rate/bin.");
  Serial.printf("sample_rate: %lu Hz, channels: %d, bin: %u\n",
                (unsigned long)cfg.sample_rate, cfg.channels,
                (unsigned)currentBinSize());
  Serial.printf("raw request: %u bytes, expected output: %u bytes\n",
                (unsigned)kRawBufferSizeBytes, (unsigned)expectedOutputBytes());
  Serial.printf("frame size: %u results, frame count: %u\n",
                (unsigned)cfg.buffer_size, (unsigned)cfg.buffer_count);
  Serial.printf("effective ADC rate: %u Hz\n",
                (unsigned)(cfg.sample_rate * cfg.channels));

  if ((cfg.sample_rate * cfg.channels) < SOC_ADC_SAMPLE_FREQ_THRES_LOW ||
      (cfg.sample_rate * cfg.channels) > SOC_ADC_SAMPLE_FREQ_THRES_HIGH) {
    Serial.printf("skipping unsupported effective ADC rate: %u Hz\n",
                  (unsigned)(cfg.sample_rate * cfg.channels));
    return false;
  }

  if (!analog_in.begin(cfg)) {
    Serial.println("analog_in.begin failed");
    return false;
  }

  delay(50);
  resetStats();
  last_report_ms = millis();
  scenario_started_ms = last_report_ms;
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
  }
}

void loop() {
  uint32_t now = millis();
  if (now - scenario_started_ms >= kScenarioDurationMs) {
    do {
      advanceScenario();
    } while (!applyScenario());
    return;
  }

  int avail_before = analog_in.available();
  size_t bytes_read = binned_stream.readBytes(raw_buffer, kRawBufferSizeBytes);
  int avail_after = analog_in.available();

  iterations++;
  if (bytes_read != expectedOutputBytes()) {
    short_reads++;
  }

  if (bytes_read < min_bytes_read) min_bytes_read = bytes_read;
  if (bytes_read > max_bytes_read) max_bytes_read = bytes_read;
  if (avail_before < min_avail_before) min_avail_before = avail_before;
  if (avail_before > max_avail_before) max_avail_before = avail_before;
  if (avail_after < min_avail_after) min_avail_after = avail_after;
  if (avail_after > max_avail_after) max_avail_after = avail_after;

  now = millis();
  if (now - last_report_ms >= kReportIntervalMs) {
    Serial.printf(
        "rate=%lu bin=%u iter=%lu short_reads=%lu bytes[min/max]=%u/%u "
        "avail_before[min/max]=%d/%d avail_after[min/max]=%d/%d\n",
        (unsigned long)currentSampleRate(), (unsigned)currentBinSize(),
        (unsigned long)iterations, (unsigned long)short_reads,
        (unsigned)min_bytes_read, (unsigned)max_bytes_read, min_avail_before,
        max_avail_before, min_avail_after, max_avail_after);
    last_report_ms = now;
  }
}

#else

void setup() {}
void loop() {}

#endif
