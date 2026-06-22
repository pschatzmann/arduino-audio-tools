#pragma once

#include "AudioToolsConfig.h"

#if defined(USE_ANALOG) && (defined(IS_ZEPHYR))

#include <vector>
#include <algorithm>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/dac.h>

#include "AudioTools/CoreAudio/AudioAnalog/AnalogDriverBase.h"
#include "AudioTools/CoreAudio/AudioTimer/AudioTimer.h"
#include "AudioTools/CoreAudio/Buffers.h"

namespace audio_tools {

class AnalogDriverZephyr : public AnalogDriverBase {
public:
  AnalogDriverZephyr() = default;
  virtual ~AnalogDriverZephyr() { end(); }

  // ============================================================
  // BEGIN / END
  // ============================================================

  bool begin(AnalogConfig cfg) override {
    TRACED();
    end();

    config = cfg;

    if (config.channels == 0 || config.bits_per_sample == 0) {
      LOGE("Invalid config");
      return false;
    }

    frame_size = config.channels * (config.bits_per_sample / 8);
    if (frame_size <= 0) {
      LOGE("Invalid frame size");
      return false;
    }

    if (config.rx_tx_mode == TX_MODE || config.rx_tx_mode == RXTX_MODE) {
      if (!setupDac()) return false;
    }

    if (config.rx_tx_mode == RX_MODE || config.rx_tx_mode == RXTX_MODE) {
      if (!setupAdc()) return false;
    }

    setupTimer();
    active = true;
    return true;
  }

  void end() override {
    timer.end();
    active = false;
    rx_buffer.resize(0);
    tx_buffer.resize(0);
  }

  // ============================================================
  // TX (APP → DAC)
  // ============================================================

  size_t write(const uint8_t* src, size_t size_bytes) override {
    if (!active || config.rx_tx_mode == RX_MODE) return 0;

    size_t bytes = (size_bytes / frame_size) * frame_size;
    if (bytes == 0) return 0;

    return tx_buffer.writeArray(src, bytes);
  }

  // ============================================================
  // RX (ADC → APP)
  // ============================================================

  size_t readBytes(uint8_t* dst, size_t size_bytes) override {
    if (!active || config.rx_tx_mode == TX_MODE) return 0;

    size_t bytes = (size_bytes / frame_size) * frame_size;
    if (bytes == 0) return 0;

    return rx_buffer.readArray(dst, bytes);
  }

  int available() override {
    return rx_buffer.available();
  }

  int availableForWrite() override {
    return tx_buffer.availableForWrite();
  }

private:

  // ============================================================
  // CONFIG + STATE
  // ============================================================

  AnalogConfig config;

  bool active = false;
  int frame_size = 0;
  int configured_channels = 0;

  // ============================================================
  // BUFFERS (FULL DUPLEX)
  // ============================================================

  RingBuffer<uint8_t> rx_buffer{0};  // ADC → APP
  RingBuffer<uint8_t> tx_buffer{0};  // APP → DAC

  // ============================================================
  // TIMING
  // ============================================================

  AudioTimer timer;

  // ============================================================
  // DEVICES
  // ============================================================

  const struct device* dac_dev = nullptr;
  const struct device* adc_dev = nullptr;

  // ============================================================
  // SETUP BUFFERS
  // ============================================================

  bool setupBuffer(RingBuffer<uint8_t>& buffer) {
    size_t size = config.buffer_count * config.buffer_size;

    buffer.resize(size);

    return buffer.address() != nullptr;
  }

  // ============================================================
  // DAC SETUP
  // ============================================================

  bool setupDac() {
#if IS_ENABLED(CONFIG_DAC)

    if (config.dac.empty()) {
      LOGE("No DAC configured");
      return false;
    }

    if (!setupBuffer(tx_buffer)){
      LOGE("TX Buffer");
      return false;
    }

    configured_channels = std::min<int>(config.channels, config.dac.size());

    for (int ch = 0; ch < configured_channels; ch++) {
      if (!device_is_ready(config.dac[ch].dev)) {
        LOGE("DAC not ready");
        return false;
      }

      int rc = dac_channel_setup_dt(&config.dac[ch]);
      if (rc != 0) {
        LOGE("DAC setup failed ch=%d rc=%d", ch, rc);
        return false;
      }
    }

    return true;
#else
    return false;
#endif
  }

  // ============================================================
  // ADC SETUP
  // ============================================================

  bool setupAdc() {
#if IS_ENABLED(CONFIG_ADC)

    if (config.adc.empty()) {
      LOGE("No ADC configured");
      return false;
    }

    if (!setupBuffer(rx_buffer)){
      LOGE("RX Buffer");
      return false;
    }

    configured_channels = std::min<int>(config.channels, config.adc.size());

    for (int ch = 0; ch < configured_channels; ch++) {
      if (!device_is_ready(config.adc[ch].dev)) {
        LOGE("ADC not ready");
        return false;
      }

      int rc = adc_channel_setup_dt(&config.adc[ch]);
      if (rc != 0) {
        LOGE("ADC setup failed ch=%d rc=%d", ch, rc);
        return false;
      }
    }

    return true;
#else
    return false;
#endif
  }

  // ============================================================
  // TIMER
  // ============================================================

  void setupTimer() {
    timer.setCallbackParameter(this);
    timer.begin(timerCallback, config.sample_rate, HZ);
  }

  static void timerCallback(void* arg) {
    auto* self = static_cast<AnalogDriverZephyr*>(arg);
    if (!self || !self->active) return;

    switch (self->config.rx_tx_mode) {
      case TX_MODE:
        self->processTx();
        break;

      case RX_MODE:
        self->processRx();
        break;

      case RXTX_MODE:
        self->processRx();  // sample first
        self->processTx();  // output second
        break;
      default:
        LOGE("Undefined rx_tx_mode: %d", (int)self->config.rx_tx_mode)
    }
  }

  // ============================================================
  // TX PIPELINE (APP → DAC)
  // ============================================================

  void processTx() {
#if IS_ENABLED(CONFIG_DAC)

    if (tx_buffer.available() < frame_size) {
      writeSilence();
      return;
    }

    uint8_t frame[64];

    tx_buffer.readArray(frame, frame_size);

    for (int ch = 0; ch < configured_channels; ch++) {
      int32_t sample = extractSample(frame, ch);
      uint32_t value = toDac(sample);

      dac_write_value_dt(&config.dac[ch], value);
    }

#endif
  }

  void writeSilence() {
#if IS_ENABLED(CONFIG_DAC)

    uint32_t mid = (1u << dacBits()) / 2u;

    for (int ch = 0; ch < configured_channels; ch++) {
      dac_write_value_dt(&config.dac[ch], mid);
    }

#endif
  }

  // ============================================================
  // RX PIPELINE (ADC → APP)
  // ============================================================

  void processRx() {
#if IS_ENABLED(CONFIG_ADC)

    if (rx_buffer.availableForWrite() < frame_size) return;

    uint8_t frame[64];

    for (int ch = 0; ch < configured_channels; ch++) {

        int16_t io = 0;

        struct adc_sequence seq = {
            .buffer = &io,
            .buffer_size = sizeof(io),
            .resolution = config.adc[ch].resolution,
        };
        adc_read_dt(&config.adc[ch], &seq);
        int32_t sample = adcToSigned(io);

        writeSample(frame, ch, sample);
    }

    rx_buffer.writeArray(frame, frame_size);

#endif
  }

  // ============================================================
  // AUDIO HELPERS
  // ============================================================

  int32_t extractSample(const uint8_t* frame, int ch) {
    int bytes = config.bits_per_sample / 8;
    const uint8_t* p = frame + ch * bytes;

    switch (config.bits_per_sample) {
      case 8:  return (int8_t)p[0];
      case 16: return *(int16_t*)p;
      case 24: return (p[0] | (p[1] << 8) | (p[2] << 16));
      default:  return *(int32_t*)p;
    }
  }

  void writeSample(uint8_t* frame, int ch, int32_t sample) {
    int bytes = config.bits_per_sample / 8;
    uint8_t* p = frame + ch * bytes;

    switch (config.bits_per_sample) {
      case 8:
        p[0] = (uint8_t)(sample >> 8);
        break;
      case 16:
        *(int16_t*)p = (int16_t)sample;
        break;
      case 24:
        p[0] = sample & 0xFF;
        p[1] = (sample >> 8) & 0xFF;
        p[2] = (sample >> 16) & 0xFF;
        break;
      default:
        *(int32_t*)p = sample;
        break;
    }
  }

  uint32_t toDac(int32_t sample) {
    int64_t min_in = -(1LL << 15);
    int64_t max_in = (1LL << 15) - 1;

    uint32_t max_out = (1u << dacBits()) - 1;

    return (uint32_t)((sample - min_in) * max_out / (max_in - min_in));
  }

  int32_t adcToSigned(int16_t raw) {
    return (int32_t)raw - 2048;
  }

  int dacBits() const {
    return (config.bits_per_sample >= 12) ? 12 : 10;
  }
};

using AnalogDriver = AnalogDriverZephyr;

} // namespace audio_tools

#endif
