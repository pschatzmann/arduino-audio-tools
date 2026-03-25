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
  cfg.sample_rate = 16000;
  cfg.channels = 2;
  cfg.buffer_size = 513;
  cfg.buffer_count = 3;
  cfg.adc_channels[0] = ADC_CHANNEL_4;
  cfg.adc_channels[1] = ADC_CHANNEL_5;

  uint32_t frame_bytes = (uint32_t)cfg.buffer_size * SOC_ADC_DIGI_RESULT_BYTES;
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
  uint32_t aligned_frame_bytes = adcContinuousAlignFrameSize(frame_bytes);
#else
  uint32_t aligned_frame_bytes = frame_bytes;
#endif

  Serial.println("ESP32 RX frame alignment test");
  Serial.printf("buffer_size: %u results\n", cfg.buffer_size);
  Serial.printf("frame bytes before alignment: %u\n", (unsigned)frame_bytes);
  Serial.printf("frame bytes after alignment: %u\n",
                (unsigned)aligned_frame_bytes);
  Serial.printf("max frame bytes: %u\n",
                (unsigned)adcContinuousMaxConvFrameBytes());

  if (aligned_frame_bytes > adcContinuousMaxConvFrameBytes()) {
    Serial.println("configuration exceeds driver frame limit");
  } else {
    analog_in.begin(cfg);
  }
}

void loop() { delay(1000); }

#else

void setup() {}
void loop() {}

#endif
