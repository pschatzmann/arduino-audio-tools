#include "Arduino.h"
#include "AudioTools.h"

#if defined(ESP32) && defined(USE_ANALOG) && !USE_LEGACY_I2S

using namespace audio_tools;

AnalogAudioStream analog_in;
uint8_t buffer[256];

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  auto cfg = analog_in.defaultConfig(RX_MODE);
  cfg.sample_rate = 8000;
  cfg.channels = 2;
  cfg.buffer_size = 128;
  cfg.buffer_count = 4;
  cfg.adc_channels[0] = ADC_CHANNEL_4;
  cfg.adc_channels[1] = ADC_CHANNEL_5;

  analog_in.begin(cfg);
  delay(50);

  Serial.println("ESP32 RX multichannel available() test");
  Serial.printf("available before read: %d\n", analog_in.available());
  size_t bytes_read = analog_in.readBytes(buffer, sizeof(buffer));
  Serial.printf("bytes_read: %u\n", (unsigned)bytes_read);
  Serial.printf("available after read: %d\n", analog_in.available());
}

void loop() { delay(1000); }

#else

void setup() {}
void loop() {}

#endif
