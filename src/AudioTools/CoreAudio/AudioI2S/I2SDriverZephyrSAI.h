#pragma once

#include "AudioToolsConfig.h"

#if defined(IS_ZEPHYR)

#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sai.h>
#include <zephyr/kernel.h>

#include "AudioTools/CoreAudio/AudioBasic/Collections.h"
#include "AudioTools/CoreAudio/AudioI2S/I2SConfigZephyr.h"
#include "AudioTools/CoreAudio/AudioI2S/I2SDriverBase.h"

#define IS_SAI_IMPLEMENTED

namespace audio_tools {

class SAIDriverZephyr : public I2SDriverBase {
  friend class I2SStream;

 public:
  I2SConfigZephyr defaultConfig(RxTxMode mode) {
    I2SConfigZephyr c(mode);
    return c;
  }

  bool setAudioInfo(AudioInfo info) {
    if (is_started) {
      if (info.equals(cfg)) return true;

      I2SConfigZephyr previous_cfg = cfg;
      cfg.sample_rate = info.sample_rate;
      cfg.channels = info.channels;
      cfg.bits_per_sample = info.bits_per_sample;

      end();
      if (begin(cfg)) return true;

      cfg = previous_cfg;
      return begin(cfg);
    }

    cfg.sample_rate = info.sample_rate;
    cfg.channels = info.channels;
    cfg.bits_per_sample = info.bits_per_sample;
    return true;
  }

  bool begin(RxTxMode mode = TX_MODE) {
    return begin(defaultConfig(mode));
  }

  bool begin(I2SConfigZephyr cfg) {
    TRACEI();

    if (is_started) end();

    const size_t slab_size =
        static_cast<size_t>(cfg.buffer_count) *
        static_cast<size_t>(cfg.buffer_size);

    if (slab_size == 0) {
      LOGE("Invalid SAI slab size");
      return false;
    }

    if (!tx_slab_buffer.resize(slab_size) ||
        !rx_slab_buffer.resize(slab_size)) {
      LOGE("Could not allocate SAI slabs");
      return false;
    }

    if (!initSlabs(cfg)) {
      LOGE("Failed to init SAI slabs");
      return false;
    }

    this->cfg = cfg;
    this->cfg.logInfo();

    if (!resolveDevice()) {
      LOGE("No SAI device found");
      return false;
    }

    if (cfg.rx_tx_mode == TX_MODE || cfg.rx_tx_mode == RXTX_MODE) {
      tx_cfg = buildSAIConfig(cfg, &tx_slab);
      if (sai_configure(sai_dev, SAI_DIR_TX, &tx_cfg) != 0) {
        LOGE("sai_configure failed TX");
        return false;
      }
    }

    if (cfg.rx_tx_mode == RX_MODE || cfg.rx_tx_mode == RXTX_MODE) {
      rx_cfg = buildSAIConfig(cfg, &rx_slab);
      if (sai_configure(sai_dev, SAI_DIR_RX, &rx_cfg) != 0) {
        LOGE("sai_configure failed RX");
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

  void end() {
    if (sai_dev == nullptr || !is_started) {
      is_started = false;
      return;
    }

    if (cfg.rx_tx_mode == TX_MODE || cfg.rx_tx_mode == RXTX_MODE) {
      sai_trigger(sai_dev, SAI_DIR_TX, SAI_TRIGGER_DROP);
    }

    if (cfg.rx_tx_mode == RX_MODE || cfg.rx_tx_mode == RXTX_MODE) {
      sai_trigger(sai_dev, SAI_DIR_RX, SAI_TRIGGER_DROP);
    }

    is_started = false;
  }

  size_t writeBytes(const void* src, size_t size_bytes) {
    if (!is_started || sai_dev == nullptr) return 0;
    if (cfg.rx_tx_mode == RX_MODE) return 0;

    int result = sai_write(sai_dev, src, size_bytes);
    return (result == 0) ? size_bytes : 0;
  }

  size_t readBytes(void* dest, size_t size_bytes) {
    if (!is_started || sai_dev == nullptr) return 0;
    if (cfg.rx_tx_mode == TX_MODE) return 0;

    size_t out_size = size_bytes;
    int result = sai_read(sai_dev, dest, &out_size);
    return (result == 0) ? out_size : 0;
  }

  int available() { return cfg.buffer_count * cfg.buffer_size; }
  int availableForWrite() { return cfg.buffer_count * cfg.buffer_size; }

 protected:
  I2SConfigZephyr cfg = defaultConfig(RXTX_MODE);

  sai_config tx_cfg{};
  sai_config rx_cfg{};

  const device* sai_dev = nullptr;
  bool is_started = false;

  Vector<uint8_t> tx_slab_buffer{0};
  Vector<uint8_t> rx_slab_buffer{0};

  k_mem_slab tx_slab;
  k_mem_slab rx_slab;

  bool initSlabs(const I2SConfigZephyr& cfg) {
    k_mem_slab_init(&tx_slab, tx_slab_buffer.data(),
                    cfg.buffer_size, cfg.buffer_count);

    k_mem_slab_init(&rx_slab, rx_slab_buffer.data(),
                    cfg.buffer_size, cfg.buffer_count);

    return true;
  }

  sai_config buildSAIConfig(const I2SConfigZephyr& cfg,
                            k_mem_slab* mem_slab) {
    sai_config c = {0};

    c.word_size = cfg.bits_per_sample;
    c.channels = cfg.channels;
    c.sample_rate = cfg.sample_rate;
    c.mem_slab = mem_slab;
    c.block_size = cfg.buffer_size;
    c.timeout = -1;

    c.master = cfg.is_master;

    return c;
  }

  bool startConfiguredStreams() {
    if (cfg.rx_tx_mode == TX_MODE) {
      return sai_trigger(sai_dev, SAI_DIR_TX, SAI_TRIGGER_START) == 0;
    }

    if (cfg.rx_tx_mode == RX_MODE) {
      return sai_trigger(sai_dev, SAI_DIR_RX, SAI_TRIGGER_START) == 0;
    }

    return sai_trigger(sai_dev, SAI_DIR_BOTH, SAI_TRIGGER_START) == 0;
  }

  bool resolveDevice() {
    if (sai_dev != nullptr) return true;

    if (cfg.dev != nullptr) {
      sai_dev = cfg.dev;
      return true;
    }

    LOGE("No SAI device configured");
    return false;
  }
};

using SAIDriver = SAIDriverZephyr;

} // namespace audio_tools

#endif
