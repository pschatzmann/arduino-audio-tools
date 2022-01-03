#pragma once

// creats __sync_synchronize() to avoid linker error for missing symbol
#define ADD_RP2040_SYNC true

#define DEFAULT_PICO_AUDIO_STATE_MACHINE 1
#define DEFAULT_PICO_AUDIO_DMA_CHANNEL 0
#define DEFAULT_PICO_AUDIO_PIO_NO 0
#define DEFAULT_PICO_AUDIO_I2S_DMA_IRQ 1

#define DEFAULT_PICO_AUDIO_I2S_DATA_PIN 26


// Active log levels
#define I2S_LOGE(...)         \
  Serial.print("E:");         \
  Serial.printf(__VA_ARGS__); \
  Serial.println()
#define I2S_LOGW(...)         \
  Serial.print("W:");         \
  Serial.printf(__VA_ARGS__); \
  Serial.println()
#define I2S_LOGI(...)         \
  Serial.print("I:");         \
  Serial.printf(__VA_ARGS__); \
  Serial.println()
// #define I2S_LOGD(...)         \
//   Serial.print("D:");         \
//   Serial.printf(__VA_ARGS__); \
//   Serial.println()

/// Suppressed Log levels
//#define I2S_LOGE(...)
//#define I2S_LOGW(...)
//#define I2S_LOGI(...)
#define I2S_LOGD(...)
