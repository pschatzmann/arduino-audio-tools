#include "Arduino.h"
#include "AudioTools.h"

#if defined(ESP32) && defined(USE_ANALOG) && !USE_LEGACY_I2S

using namespace audio_tools;

AnalogAudioStream analog_in;
uint8_t buffer[256];

uint32_t iterations = 0;
uint32_t short_reads = 0;
uint32_t last_report_ms = 0;
size_t min_bytes_read = sizeof(buffer);
size_t max_bytes_read = 0;
int min_avail_before = 0x7fffffff;
int max_avail_before = 0;
int min_avail_after = 0x7fffffff;
int max_avail_after = 0;
bool rx_active = false;

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  auto cfg = analog_in.defaultConfig(RX_MODE);
  cfg.sample_rate = 12000;
  cfg.channels = 2;
  cfg.buffer_size = 128;
  cfg.buffer_count = 4;
  cfg.adc_channels[0] = ADC_CHANNEL_4;
  cfg.adc_channels[1] = ADC_CHANNEL_5;

  Serial.println("ESP32 RX V1 multichannel soak test");
  Serial.printf("buffer_size: %u results\n", cfg.buffer_size);
  Serial.printf("buffer_count: %u frames\n", cfg.buffer_count);
  Serial.printf("expected readable bytes: %u\n",
                (unsigned)(cfg.buffer_size * sizeof(int16_t)));

  Serial.printf("effective ADC rate: %u Hz\n",
                (unsigned)(cfg.sample_rate * cfg.channels));

  rx_active = analog_in.begin(cfg);
  if (!rx_active) {
    Serial.println("analog_in.begin failed; stopping test");
    while (true) {
      delay(1000);
    }
  }
  delay(50);
  last_report_ms = millis();
}

void loop() {
  if (!rx_active) {
    delay(1000);
    return;
  }

  int avail_before = analog_in.available();
  size_t bytes_read = analog_in.readBytes(buffer, sizeof(buffer));
  int avail_after = analog_in.available();

  iterations++;
  if (bytes_read != sizeof(buffer)) {
    short_reads++;
  }

  if (bytes_read < min_bytes_read) min_bytes_read = bytes_read;
  if (bytes_read > max_bytes_read) max_bytes_read = bytes_read;
  if (avail_before < min_avail_before) min_avail_before = avail_before;
  if (avail_before > max_avail_before) max_avail_before = avail_before;
  if (avail_after < min_avail_after) min_avail_after = avail_after;
  if (avail_after > max_avail_after) max_avail_after = avail_after;

  uint32_t now = millis();
  if (now - last_report_ms >= 1000) {
    Serial.printf(
        "iter=%lu short_reads=%lu bytes[min/max]=%u/%u "
        "avail_before[min/max]=%d/%d avail_after[min/max]=%d/%d\n",
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
