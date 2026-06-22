#pragma once
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/class/usbd_uac2.h>
#include <zephyr/usb/usbd.h>

#include <cstring>
#include <functional>
#include <vector>

#include "AudioTools/Communication/USB/USBAudioConfig.h"

LOG_MODULE_DECLARE(usb_audio_device_zephyr, LOG_LEVEL_INF);

namespace audio_tools {

/**
 * @brief USB Audio 2.0 device backend for Zephyr RTOS.
 *
 * Uses Zephyr's native `usbd_uac2` API. The USB descriptor is fixed at
 * build time via the board device tree; dynamic re-enumeration is not
 * supported.  Conforms to the duck-typing interface expected by
 * `USBAudioStream<Device>` — no TinyUSB dependency.
 *
 * DTS requirements:
 *   - Alias `uac2_dev` pointing to the UAC2 node.
 *   - TX_MODE: ISO IN endpoint (device → host).
 *   - RX_MODE: ISO OUT endpoint (host → device), optional explicit feedback EP.
 *   - `CONFIG_USBD_UAC2=y`, `CONFIG_USB_DEVICE_STACK_NEXT=y`.
 *
 * @ingroup io
 * @ingroup usb
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class USBAudioDeviceZephyr {
 public:
  USBAudioDeviceZephyr() = default;
  USBAudioDeviceZephyr(USBAudioConfig cfg) : config_(cfg) {}

  // ── Duck-typing interface required by USBAudioStream<Device> ───────────────

  USBAudioConfig defaultConfig(RxTxMode mode = RXTX_MODE) {
    USBAudioConfig cfg;
    cfg.sample_rate = 48000;
    cfg.channels = 2;
    cfg.bits_per_sample = 16;
    cfg.enable_ep_in = (mode == TX_MODE || mode == RXTX_MODE);
    cfg.enable_ep_out = (mode == RX_MODE || mode == RXTX_MODE);
    cfg.enable_feedback_ep = (mode == RX_MODE);
    cfg.terminal_id = 1;
    return cfg;
  }

  bool begin(const USBAudioConfig& cfg) {
    config_ = cfg;
    return begin();
  }

  bool begin() {
    uint16_t frame = audioPacketSize();
    block_sz_ = (uint16_t)((frame + 3u) & ~3u);  // round up to 4-byte boundary
    uint8_t count = (uint8_t)(kPipelineDepth * 2u);

    slab_buf_.resize((size_t)block_sz_ * count);
    if (k_mem_slab_init(&slab_, slab_buf_.data(), block_sz_, count) != 0) {
      LOG_ERR("USBAudioDeviceZephyr: k_mem_slab_init failed");
      slab_buf_.resize(0);
      return false;
    }

    uac2_dev_ = DEVICE_DT_GET(DT_ALIAS(uac2_dev));
    if (!device_is_ready(uac2_dev_)) {
      LOG_ERR("USBAudioDeviceZephyr: UAC2 device not ready");
      slab_buf_.resize(0);
      return false;
    }

    static const struct uac2_ops ops = {
        .terminal_update_cb = s_terminal_update,
        .get_recv_buf = s_get_recv_buf,
        .data_recv_cb = s_data_recv,
        .buf_release_cb = s_buf_release,
        .get_explicit_feedback = s_get_feedback,
    };
    usbd_uac2_set_ops(uac2_dev_, &ops, this);

    is_active_ = true;
    LOG_INF("USBAudioDeviceZephyr: started (%u Hz, %u ch, %u bit)",
            config_.sample_rate, config_.channels, config_.bits_per_sample);
    return true;
  }

  void end() {
    is_active_ = false;
    streaming_ = false;
    slab_buf_.resize(0);
    LOG_INF("USBAudioDeviceZephyr: stopped");
  }

  void setRxCallback(std::function<void(const uint8_t*, uint16_t)> cb) {
    rx_cb_ = std::move(cb);
  }

  void setTxCallback(std::function<uint16_t(uint8_t*, uint16_t)> cb) {
    tx_cb_ = std::move(cb);
  }

  void process() { k_yield(); }

  uint16_t audioPacketSize() const {
    return (uint16_t)((config_.sample_rate / 1000U) * config_.channels *
                      (config_.bits_per_sample / 8U));
  }

  /// Zephyr UAC2 descriptors are fixed in the device tree at build time.
  /// This method logs a warning if the audio format changes and is otherwise
  /// a no-op.
  void reenumerateUSBOnChange(const USBAudioConfig& cfg) {
    if (cfg.sample_rate != config_.sample_rate ||
        cfg.channels != config_.channels ||
        cfg.bits_per_sample != config_.bits_per_sample) {
      LOG_WRN(
          "USBAudioDeviceZephyr: audio format change ignored — "
          "DTS descriptor is fixed at build time");
    }
  }

  /// True while the USB host is actively streaming.
  bool mounted() const { return streaming_; }

  /** @brief Returns the TX audio buffer.  Override in platform subclasses
   *  to provide a cross-core safe implementation (e.g. BufferRTOS on ESP32).
   *  The default RingBuffer is suitable for single-core platforms (RP2040). */
  BaseBuffer<uint8_t>& bufferTx() override { return default_buffer_tx_; }

  /** @brief Returns the RX audio buffer.  Same override rules as bufferTx(). */
  BaseBuffer<uint8_t>& bufferRx() override { return default_buffer_rx_; }

 private:
  // ── Default audio buffers (RingBuffer, suitable for single-core platforms) ─
  RingBuffer<uint8_t> default_buffer_tx_{1};
  RingBuffer<uint8_t> default_buffer_rx_{1};

  // ── Static trampolines (user_data == this) ────────────────────────────────

  static void s_terminal_update(const struct device* /*dev*/,
                                uint8_t /*terminal*/, bool enabled,
                                bool /*microframes*/, void* user_data) {
    auto* self = static_cast<USBAudioDeviceZephyr*>(user_data);
    self->streaming_ = enabled;
    if (enabled && self->config_.enable_ep_in) {
      self->primeTxPipeline();
    }
    LOG_INF("USBAudioDeviceZephyr: streaming %s",
            enabled ? "started" : "stopped");
  }

  static void* s_get_recv_buf(const struct device* /*dev*/,
                              uint8_t /*terminal*/, uint16_t /*size*/,
                              void* user_data) {
    auto* self = static_cast<USBAudioDeviceZephyr*>(user_data);
    if (!self->is_active_ || !self->config_.enable_ep_out) return nullptr;
    void* buf = nullptr;
    if (k_mem_slab_alloc(&self->slab_, &buf, K_NO_WAIT) != 0) {
      LOG_WRN("USBAudioDeviceZephyr: slab alloc failed (overrun)");
      return nullptr;
    }
    return buf;
  }

  static void s_data_recv(const struct device* /*dev*/, uint8_t /*terminal*/,
                          void* buf, uint16_t size, void* user_data) {
    auto* self = static_cast<USBAudioDeviceZephyr*>(user_data);
    if (!self->is_active_ || !buf) {
      if (buf) k_mem_slab_free(&self->slab_, buf);
      return;
    }
    if (self->rx_cb_) {
      self->rx_cb_(static_cast<const uint8_t*>(buf), size);
    }
    k_mem_slab_free(&self->slab_, buf);
  }

  static void s_buf_release(const struct device* /*dev*/, uint8_t /*terminal*/,
                            void* buf, void* user_data) {
    auto* self = static_cast<USBAudioDeviceZephyr*>(user_data);
    if (!buf) return;
    k_mem_slab_free(&self->slab_, buf);

    if (!self->is_active_ || !self->streaming_ || !self->tx_cb_) return;

    void* next = nullptr;
    if (k_mem_slab_alloc(&self->slab_, &next, K_NO_WAIT) != 0) return;

    uint16_t filled =
        self->tx_cb_(static_cast<uint8_t*>(next), self->block_sz_);
    if (filled < self->block_sz_) {
      memset(static_cast<uint8_t*>(next) + filled, 0,
             (size_t)(self->block_sz_ - filled));
    }
    if (usbd_uac2_send(self->uac2_dev_, self->config_.terminal_id, next,
                       self->block_sz_) != 0) {
      k_mem_slab_free(&self->slab_, next);
    }
  }

  static uint32_t s_get_feedback(const struct device* /*dev*/,
                                 uint8_t /*terminal*/, void* user_data) {
    auto* self = static_cast<USBAudioDeviceZephyr*>(user_data);
    uint32_t spf = self->config_.sample_rate / 1000U;
    return (spf << 14) & 0x00FFFFFFu;
  }

  // ── TX pipeline priming ───────────────────────────────────────────────────

  void primeTxPipeline() {
    if (!tx_cb_) return;
    for (uint8_t i = 0; i < kPipelineDepth; ++i) {
      void* buf = nullptr;
      if (k_mem_slab_alloc(&slab_, &buf, K_NO_WAIT) != 0) break;
      uint16_t filled = tx_cb_(static_cast<uint8_t*>(buf), block_sz_);
      if (filled < block_sz_) {
        memset(static_cast<uint8_t*>(buf) + filled, 0,
               (size_t)(block_sz_ - filled));
      }
      if (usbd_uac2_send(uac2_dev_, config_.terminal_id, buf, block_sz_) != 0) {
        k_mem_slab_free(&slab_, buf);
        break;
      }
    }
  }

  // ── Members ───────────────────────────────────────────────────────────────

  USBAudioConfig config_;
  const struct device* uac2_dev_ = nullptr;
  struct k_mem_slab slab_{};
  std::vector<uint8_t> slab_buf_;
  bool is_active_ = false;
  bool streaming_ = false;
  uint16_t block_sz_ = 0;

  std::function<void(const uint8_t*, uint16_t)> rx_cb_;
  std::function<uint16_t(uint8_t*, uint16_t)> tx_cb_;

  static constexpr uint8_t kPipelineDepth = 3;
};

/**
 * @brief USBAudioStream type alias for Zephyr RTOS.  
 * @ingroup usb
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
using USBAudioStream = USBAudioDeviceZephyr;

}  // namespace audio_tools
