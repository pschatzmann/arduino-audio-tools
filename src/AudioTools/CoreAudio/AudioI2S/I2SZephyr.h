#pragma once

#include "AudioToolsConfig.h"

#if defined(IS_ZEPHYR) || defined(DOXYGEN)

#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/kernel.h>

#include "AudioTools/CoreAudio/AudioBasic/Collections.h"
#include "AudioTools/CoreAudio/AudioI2S/I2SConfigZephyr.h"

#define IS_I2S_IMPLEMENTED

namespace audio_tools {

/**
 * @brief Basic I2S API for Zephyr based targets.
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SDriverZephyr {
  friend class I2SStream;

 public:
  /// Provides the default configuration
  I2SConfigZephyr defaultConfig(RxTxMode mode) {
    I2SConfigZephyr c(mode);
    return c;
  }

  /// Potentially updates the sample rate (if supported)
  bool setAudioInfo(AudioInfo info) {
    if (is_started) {
      if (info.equals(cfg)) return true;

      I2SConfigZephyr previous_cfg = cfg;
      cfg.sample_rate = info.sample_rate;
      cfg.channels = info.channels;
      cfg.bits_per_sample = info.bits_per_sample;

      end();
      if (begin(cfg)) {
        return true;
      }

      cfg = previous_cfg;
      return begin(cfg);
    }

    cfg.sample_rate = info.sample_rate;
    cfg.channels = info.channels;
    cfg.bits_per_sample = info.bits_per_sample;
    return true;
  }

  /// starts the DAC with the default config
  bool begin(RxTxMode mode = TX_MODE) { return begin(defaultConfig(mode)); }

  /// starts the DAC with the current config
  bool begin(I2SConfigZephyr cfg) {
    TRACEI();

    if (is_started) end();

    const size_t slab_size = static_cast<size_t>(cfg.buffer_count) *
                             static_cast<size_t>(cfg.buffer_size);
    if (slab_size == 0) {
      LOGE("Invalid Zephyr I2S slab size");
      return false;
    }

    if (!tx_slab_buffer.resize(slab_size) ||
        !rx_slab_buffer.resize(slab_size)) {
      LOGE("Could not resize Zephyr I2S slabs");
      return false;
    }

    if (!initSlabs(cfg)) {
      LOGE("Failed to initialize Zephyr I2S slabs");
      return false;
    }

    this->cfg = cfg;
    this->cfg.logInfo();

    if (!resolveDevice()) {
      LOGE("No Zephyr I2S device found");
      return false;
    }

    if (cfg.rx_tx_mode == TX_MODE || cfg.rx_tx_mode == RXTX_MODE) {
      tx_cfg = buildI2SConfig(cfg, &tx_slab);
      if (i2s_configure(i2s_dev, I2S_DIR_TX, &tx_cfg) != 0) {
        LOGE("i2s_configure failed for TX");
        return false;
      }
    }

    if (cfg.rx_tx_mode == RX_MODE || cfg.rx_tx_mode == RXTX_MODE) {
      rx_cfg = buildI2SConfig(cfg, &rx_slab);
      if (i2s_configure(i2s_dev, I2S_DIR_RX, &rx_cfg) != 0) {
        LOGE("i2s_configure failed for RX");
        return false;
      }
    }

    if (!startConfiguredStreams()) {
      end();
      return false;
    }

    is_started = true;
    return true;
  }

  /// stops the I2S interface
  void end() {
    if (i2s_dev == nullptr || !is_started) {
      is_started = false;
      return;
    }

    if (cfg.rx_tx_mode == TX_MODE || cfg.rx_tx_mode == RXTX_MODE) {
      i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_DROP);
      i2s_config disable_cfg = tx_cfg;
      disable_cfg.frame_clk_freq = 0U;
      i2s_configure(i2s_dev, I2S_DIR_TX, &disable_cfg);
    }

    if (cfg.rx_tx_mode == RX_MODE || cfg.rx_tx_mode == RXTX_MODE) {
      i2s_trigger(i2s_dev, I2S_DIR_RX, I2S_TRIGGER_DROP);
      i2s_config disable_cfg = rx_cfg;
      disable_cfg.frame_clk_freq = 0U;
      i2s_configure(i2s_dev, I2S_DIR_RX, &disable_cfg);
    }

    is_started = false;
  }

  /// provides the actual configuration
  I2SConfigZephyr config() { return cfg; }

  /// writes the data to the I2S interface
  size_t writeBytes(const void* src, size_t size_bytes) {
    if (!is_started || i2s_dev == nullptr) return 0;
    if (cfg.rx_tx_mode == RX_MODE) return 0;

    int result = i2s_buf_write(i2s_dev, const_cast<void*>(src), size_bytes);
    return (result == 0) ? size_bytes : 0;
  }

  size_t readBytes(void* dest, size_t size_bytes) {
    if (!is_started || i2s_dev == nullptr) return 0;
    if (cfg.rx_tx_mode == TX_MODE) return 0;

    size_t result_size = size_bytes;
    int result = i2s_buf_read(i2s_dev, dest, &result_size);
    return (result == 0) ? result_size : 0;
  }

  int available() { return I2S_BUFFER_COUNT * I2S_BUFFER_SIZE; }

  int availableForWrite() { return I2S_BUFFER_COUNT * I2S_BUFFER_SIZE; }

 protected:
  I2SConfigZephyr cfg = defaultConfig(RXTX_MODE);
  i2s_config tx_cfg{};
  i2s_config rx_cfg{};
  const device* i2s_dev = nullptr;
  bool is_started = false;
  Vector<uint8_t> tx_slab_buffer{0};
  Vector<uint8_t> rx_slab_buffer{0};
  k_mem_slab tx_slab;
  k_mem_slab rx_slab;
  bool slabs_initialized = false;

  i2s_config buildI2SConfig(const I2SConfigZephyr& cfg, k_mem_slab* mem_slab) {
    i2s_config i2s_cfg = {0};
    i2s_cfg.word_size = cfg.bits_per_sample;
    i2s_cfg.channels = cfg.channels;
    i2s_cfg.format = I2S_FMT_DATA_FORMAT_I2S;
    i2s_cfg.options =
        cfg.is_master
            ? (I2S_OPT_BIT_CLK_CONTROLLER | I2S_OPT_FRAME_CLK_CONTROLLER)
            : (I2S_OPT_BIT_CLK_TARGET | I2S_OPT_FRAME_CLK_TARGET);
    i2s_cfg.frame_clk_freq = cfg.sample_rate;
    i2s_cfg.mem_slab = mem_slab;
    i2s_cfg.block_size = I2S_BUFFER_SIZE;
    i2s_cfg.timeout = -1;
    return i2s_cfg;
  }

  bool initSlabs(const I2SConfigZephyr& cfg) {
    k_mem_slab_init(&tx_slab, tx_slab_buffer.data(), cfg.buffer_size,
                    cfg.buffer_count);
    k_mem_slab_init(&rx_slab, rx_slab_buffer.data(), cfg.buffer_size,
                    cfg.buffer_count);
    slabs_initialized = true;
    return true;
  }

  bool startConfiguredStreams() {
    if (cfg.rx_tx_mode == TX_MODE) {
      return i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_START) == 0;
    }

    if (cfg.rx_tx_mode == RX_MODE) {
      return i2s_trigger(i2s_dev, I2S_DIR_RX, I2S_TRIGGER_START) == 0;
    }

    int result = i2s_trigger(i2s_dev, I2S_DIR_BOTH, I2S_TRIGGER_START);
    if (result == 0) return true;
    if (result != -ENOSYS) return false;

    return i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_START) == 0 &&
           i2s_trigger(i2s_dev, I2S_DIR_RX, I2S_TRIGGER_START) == 0;
  }

  bool resolveDevice() {
    if (i2s_dev != nullptr) {
      return true;
    }

    if (cfg.dev != nullptr) {
      LOGI("Using provided I2S device");
      i2s_dev = cfg.dev;
      return true;
    }

    LOGE("I2S device is not set in configuration");
    return false;
  }
};

using I2SDriver = I2SDriverZephyr;

}  // namespace audio_tools

#endif
