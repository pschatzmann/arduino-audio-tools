#include "Arduino.h"
#include "AudioTools.h"

#if defined(ESP32) && defined(USE_ANALOG) && !USE_LEGACY_I2S

using namespace audio_tools;

AnalogAudioStream analog_in;

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  auto cfg = analog_in.defaultConfig(RX_MODE);
  cfg.sample_rate = 12000;
  cfg.channels = 1;
  cfg.buffer_size = 256;
  cfg.buffer_count = 5;
  cfg.adc_channels[0] = ADC_CHANNEL_4;

  uint32_t frame_bytes = (uint32_t)cfg.buffer_size * SOC_ADC_DIGI_RESULT_BYTES;
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
  frame_bytes = adcContinuousAlignFrameSize(frame_bytes);
#endif
  uint32_t expected_pool_bytes = frame_bytes * cfg.buffer_count;

  Serial.println("ESP32 RX buffer_count test");
  Serial.printf("buffer_size: %u results\n", cfg.buffer_size);
  Serial.printf("buffer_count: %u frames\n", cfg.buffer_count);
  Serial.printf("aligned frame bytes: %u\n", (unsigned)frame_bytes);
  Serial.printf("expected RX pool bytes: %u\n", (unsigned)expected_pool_bytes);

  analog_in.begin(cfg);
}

void loop() { delay(1000); }

#else

void setup() {}
void loop() {}

#endif
