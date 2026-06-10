/**
 * @file USBAudioStream.h
 */

#pragma once

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/class/usbd_uac2.h>
#include <zephyr/usb/usbd.h>

#include <cstdint>
#include <cstring>

#include "AudioConfig.h"
#include "AudioTools/CoreAudio/BaseStream.h"

namespace audio_tools {

/**
 * USBAudioConfig
 **/

struct USBAudioConfig : public AudioInfo {
  RxTxMode rx_tx_mode = TX_MODE;

  /** size of slab (preallocated buffer of fixed size blocks) */
  size_t slab_size_bytes =
      1024;  ///< total slab memory; actual size rounds up to block boundary

  /**
   * Terminal ID reported to UAC2 callbacks.
   * TX_MODE: Output Terminal ID (device → host, ISO IN).
   * RX_MODE: Input Terminal ID  (host → device, ISO OUT).
   * Must match your devicetree UAC2 node topology.
   */
  uint8_t terminal_id = 1;

  /**
   * Enable explicit feedback endpoint (RX_MODE only).
   * Requires a feedback EP in DTS.
   */
  bool explicit_feedback = true;

  /**
   * Async mode: write()/readBytes() return immediately with however
   * many bytes fit. Sync mode: block (with 1 ms yields) until all
   * bytes are processed.
   */
  bool is_async = true;
};

/**
 * @brief USB Audio 2.0 Stream for Zephyr
 *
 * Supports TX_MODE (app calls write(), device sends audio TO the USB host)
 * and RX_MODE (app calls readBytes(), device receives audio FROM the USB host).
 *
 * There are 2 different integratin paths: you can call write()/readBytes()
 * directly on USBAudioStream, or you can set an external Stream  as the
 * source/sink via setStream() and write()/readBytes() will proxy to that. The
 * latter allows you to use USBAudioStream as a drop-in USB audio backend for
 * existing code that already writes to or reads from a Stream. 
 *
 * TX_MODE data flow (USB capture source / microphone):
 *   write()               [app → ring]
 *     → _sendFromRing()   [ring → slab block → usbd_uac2_send()]
 *     → s_buf_release()   [UAC2 done with block → free slab]
 *                          ↑ re-triggers _sendFromRing() to keep pipeline full
 *
 * RX_MODE data flow (USB playback sink / speaker):
 *   USB host ISO OUT
 *     → s_get_recv_buf()  [alloc slab block]
 *     → s_data_recv()     [copy slab → ring, free slab]
 *     → readBytes()       [app drains ring]
 *
 * DTS requirements:
 *   - zephyr,uac2 node with alias "uac2_dev"
 *   - TX_MODE: ISO IN endpoint  (device → host)
 *   - RX_MODE: ISO OUT endpoint (host → device), optional explicit feedback EP
 *   - CONFIG_USBD_UAC2=y, CONFIG_USB_DEVICE_STACK_NEXT=y
 *   - CONFIG_HEAP_MEM_POOL_SIZE >= 8192
 **/

class USBAudioStream : public AudioStream {
 public:
  USBAudioStream() = default;
  USBAudioStream(Print& out) { setOutput(out); }
  USBAudioStream(Stream& io) { setStream(io); }

  ~USBAudioStream() { end(); }

  USBAudioConfig defaultConfig(RxTxMode mode = TX_MODE) {
    USBAudioConfig cfg;
    cfg.rx_tx_mode = mode;
    cfg.sample_rate = 48000;
    cfg.channels = 2;
    cfg.bits_per_sample = 16;
    // cfg.slab_block_count  = 6;
    cfg.slab_size_bytes =
        1024;  ///< total slab memory; actual size rounds up to block boundary

    cfg.explicit_feedback = (mode == RX_MODE);
    cfg.is_async = true;
    return cfg;
  }

  bool begin() {
    if (!cfg_) {
      LOG_ERR("USBAudioStream: invalid config");
      return false;
    }
    return begin(cfg_);
  }

  bool begin(USBAudioConfig cfg) {
    if (!cfg) {
      LOG_ERR(
          "USBAudioStream: invalid AudioInfo "
          "(rate=%u ch=%u bits=%u)",
          cfg.sample_rate, cfg.channels, cfg.bits_per_sample);
      return false;
    }

    cfg_ = cfg;
    size_t block_sz = ROUND_UP(nominalFrameBytes(), 4);
    uint8_t slab_block_count = (cfg_.slab_size_bytes + block_sz - 1) / block_sz;
    slab_block_count = MAX(slab_block_count, 4);  // enforce minimum

    /* Ring buffer: slab_block_count x 2 frames of headroom */
    size_t frame = nominalFrameBytes();
    size_t ring_cap = frame * slab_block_count * 2;
    if (!ring_.resize(ring_cap)) {
      LOG_ERR("USBAudioStream: ring buffer alloc failed");
      return false;
    }

    /* Slab – used in both modes:
     *   TX_MODE: UAC2 stack writes USB OUT data into slab blocks.
     *   RX_MODE: we write slab blocks into usbd_uac2_send(). */
    size_t block_sz = ROUND_UP(frame, 4);
    if (!slab_buf_.resize(block_sz * slab_block_count)) {
      LOG_ERR("USBAudioStream: slab alloc failed");
      ring_.resize(0);
      return false;
    }
    if (k_mem_slab_init(&slab_, slab_buf_.data(), block_sz, slab_block_count) !=
        0) {
      LOG_ERR("USBAudioStream: k_mem_slab_init failed");
      slab_buf_.resize(0);
      ring_.resize(0);
      return false;
    }

    uac2_dev_ = DEVICE_DT_GET(DT_ALIAS(uac2_dev));
    if (!device_is_ready(uac2_dev_)) {
      LOG_ERR("USBAudioStream: UAC2 device not ready");
      cleanup();
      return false;
    }

    static const struct uac2_ops ops = {
        .terminal_update_cb = s_terminal_update,
        .get_recv_buf = s_get_recv_buf,          /* RX only, no-op in TX */
        .data_recv_cb = s_data_recv,             /* RX only, no-op in TX */
        .buf_release_cb = s_buf_release,         /* TX: send-complete    */
        .get_explicit_feedback = s_get_feedback, /* RX only              */
    };
    usbd_uac2_set_ops(uac2_dev_, &ops, this);

    is_active_ = true;
    LOG_INF("USBAudioStream: started (%s, %u Hz, %u ch, %u bit)",
            cfg_.rx_tx_mode == TX_MODE ? "TX/source" : "RX/sink",
            cfg_.sample_rate, cfg_.channels, cfg_.bits_per_sample);
    return true;
  }

  void end() {
    if (!is_active_) return;
    is_active_ = false;
    streaming_ = false;
    cleanup();
    LOG_INF("USBAudioStream: stopped");
  }

  void setAudioInfo(AudioInfo info) override {
    if (!info) return;
    AudioStream::setAudioInfo(info);
    bool changed = (static_cast<AudioInfo>(cfg_) != info);
    cfg_.copyFrom(info);
    if (changed && is_active_) {
      LOG_INF("USBAudioStream: format changed - restarting");
      end();
      begin(cfg_);
    }
  }

  AudioInfo audioInfo() const { return static_cast<AudioInfo>(cfg_); }

  /* ── Stream interface ────────────────────────────────────────────── */

  /**
   * TX_MODE: push PCM data toward the USB host.
   *
   * Data is written into the ring buffer. _sendFromRing() is then
   * called to immediately hand as many complete slab blocks as possible
   * to usbd_uac2_send(). As each send completes, s_buf_release() frees
   * the slab block and calls _sendFromRing() again, keeping the ISO IN
   * pipeline full without any polling thread.
   */
  size_t write(const uint8_t* data, size_t len) override {
    if (!is_active_ || !data || len == 0) return 0;
    if (cfg_.rx_tx_mode == RX_MODE) {
      LOG_WRN("USBAudioStream::write called in RX_MODE - ignored");
      return 0;
    }

    if (cfg_.is_async) {
      size_t written = ring_.writeArray(data, len);
      _sendFromRing();
      return written;
    }

    /* Sync: block until all bytes are queued */
    size_t total = 0;
    while (total < len) {
      total += ring_.writeArray(data + total, len - total);
      _sendFromRing();
      if (total < len) k_sleep(K_MSEC(1));
    }
    return total;
  }

  /**
   * RX_MODE: read PCM data received from the USB host.
   * Data arrives via UAC2 OUT callbacks into the ring buffer.
   */
  size_t readBytes(uint8_t* data, size_t len) override {
    if (!is_active_ || !data || len == 0) return 0;
    if (cfg_.rx_tx_mode == TX_MODE) {
      LOG_WRN("USBAudioStream::readBytes called in TX_MODE - ignored");
      return 0;
    }

    if (cfg_.is_async) {
      return ring_.readArray(data, len);
    }

    /* Sync: block until all bytes are delivered */
    size_t total = 0;
    while (total < len) {
      total += ring_.readArray(data + total, len - total);
      if (total < len) k_sleep(K_MSEC(1));
    }
    return total;
  }

  int available() override { return static_cast<int>(ring_.available()); }
  int availableForWrite() override {
    return static_cast<int>(ring_.availableForWrite());
  }
  void flush() override { /* UAC2 is push-driven */ }

  explicit operator bool() const { return is_active_; }
  bool isActive() const { return is_active_; }
  bool isStreaming() const { return streaming_; }

  uint32_t rxFrames() const { return rx_frames_; }
  uint32_t overruns() const { return overruns_; }
  uint32_t underruns() const { return underruns_; }

  void setOutput(Print& out) { p_output = &out; }
  void setStream(Stream& io) {
    p_output = &io;
    p_stream = &io;
  }
  void setOutput(Stream& io) { setStream(io); }

 protected:
  USBAudioConfig cfg_{};
  const struct device* uac2_dev_ = nullptr;
  struct k_mem_slab slab_{};
  Vector<uint8_t> slab_buf_{0};
  RingBuffer<uint8_t> ring_{0};
  bool is_active_ = false;
  bool streaming_ = false;
  uint32_t rx_frames_ = 0;
  uint32_t overruns_ = 0;
  uint32_t underruns_ = 0;
  Print* p_output = nullptr;
  Stream* p_stream = nullptr;

  size_t nominalFrameBytes() const {
    return (cfg_.sample_rate / 1000U) * cfg_.channels *
           (cfg_.bits_per_sample / 8U);
  }

  void cleanup() {
    slab_buf_.resize(0);
    ring_.resize(0);
  }

  /**
   * TX_MODE only: drain the ring into usbd_uac2_send() calls.
   *
   * Each call submits at most one slab block per iteration to avoid
   * starving the slab. s_buf_release() re-invokes this after each
   * transfer completes, so the ISO IN endpoint stays fed without a
   * polling thread.
   *
   * Safe to call from both application context (write()) and UAC2
   * callback context (s_buf_release()).
   */
  void _sendFromRing() {
    if (!is_active_ || !streaming_) return;

    size_t block_sz = ROUND_UP(nominalFrameBytes(), 4);

    while (ring_.available() >= block_sz) {
      void* buf = nullptr;
      if (k_mem_slab_alloc(&slab_, &buf, K_NO_WAIT) != 0) {
        /* All slab blocks are in-flight - ISO IN pipeline is full. */
        break;
      }
      size_t read = 0;

      if (p_stream) {
        read = p_stream.readBytes(static_cast<uint8_t*>(buf), block_sz);
      } else {
        read = ring_.readArray(static_cast<uint8_t*>(buf), block_sz);
      }
      if (read < block_sz) {
        /* Zero-pad partial read to prevent ISO IN NAK glitch. */
        memset(static_cast<uint8_t*>(buf) + read, 0, block_sz - read);
        underruns_++;
      }

      int ret = usbd_uac2_send(uac2_dev_, cfg_.terminal_id, buf,
                               static_cast<uint16_t>(block_sz));
      if (ret != 0) {
        LOG_WRN("usbd_uac2_send failed: %d", ret);
        k_mem_slab_free(&slab_, buf);
        overruns_++;
        break;
      }
    }
  }

  /* ── Static trampolines ──────────────────────────────────────────── */

  static void s_terminal_update(const struct device* dev, uint8_t terminal,
                                bool enabled, bool microframes,
                                void* user_data) {
    ARG_UNUSED(dev);
    ARG_UNUSED(terminal);
    ARG_UNUSED(microframes);
    auto* self = static_cast<USBAudioStream*>(user_data);
    self->streaming_ = enabled;
    if (enabled) {
      LOG_INF("USBAudioStream: host started streaming");
      /* TX_MODE: prime the ISO IN pipeline immediately */
      if (self->cfg_.rx_tx_mode == TX_MODE) {
        self->_sendFromRing();
      }
    } else {
      self->ring_.reset();
      LOG_INF("USBAudioStream: host stopped streaming");
    }
  }

  /**
   * RX_MODE only: called by UAC2 to obtain a buffer before filling it
   * with USB ISO OUT data.
   * TX_MODE: not invoked for ISO IN; returns nullptr defensively.
   */
  static void* s_get_recv_buf(const struct device* dev, uint8_t terminal,
                              uint16_t size, void* user_data) {
    ARG_UNUSED(dev);
    ARG_UNUSED(terminal);
    auto* self = static_cast<USBAudioStream*>(user_data);

    if (!self->is_active_ || self->cfg_.rx_tx_mode != RX_MODE) return nullptr;

    void* buf = nullptr;
    if (k_mem_slab_alloc(&self->slab_, &buf, K_NO_WAIT) != 0) {
      self->overruns_++;
      return nullptr;
    }
    /* Reject oversized frames (feedback drift adds at most one sample) */
    size_t max_sz = self->nominalFrameBytes() +
                    (self->cfg_.channels * self->cfg_.bits_per_sample / 8);
    if (size > max_sz) {
      k_mem_slab_free(&self->slab_, buf);
      return nullptr;
    }
    return buf;
  }

  /**
   * RX_MODE only: UAC2 has filled the buffer with USB audio data.
   * Copy into the ring buffer then free the slab block.
   */
  static void s_data_recv(const struct device* dev, uint8_t terminal, void* buf,
                          uint16_t size, void* user_data) {
    ARG_UNUSED(dev);
    ARG_UNUSED(terminal);
    auto* self = static_cast<USBAudioStream*>(user_data);

    if (!self->is_active_ || !buf) {
      if (buf) k_mem_slab_free(&self->slab_, buf);
      return;
    }

    self->rx_frames_++;
    size_t written = 0;
    if (self->p_output) {
      written = self->p_output.write(static_cast<const uint8_t*>(buf), size);

    } else {
      written = self->ring_.writeArray(static_cast<const uint8_t*>(buf), size);
    }
    if (written < size) self->underruns_++;

    k_mem_slab_free(&self->slab_, buf);
  }

  /**
   * TX_MODE: called by UAC2 after a buffer submitted via usbd_uac2_send()
   * has been transferred to the host (or aborted). Free the slab block
   * and immediately queue the next one so the ISO IN endpoint never stalls.
   *
   * RX_MODE: not invoked in normal operation (buf was already freed in
   * s_data_recv); this is a defensive no-op.
   */
  static void s_buf_release(const struct device* dev, uint8_t terminal,
                            void* buf, void* user_data) {
    ARG_UNUSED(dev);
    ARG_UNUSED(terminal);
    auto* self = static_cast<USBAudioStream*>(user_data);

    if (!buf) return;

    if (self->cfg_.rx_tx_mode == TX_MODE) {
      k_mem_slab_free(&self->slab_, buf);
      /* Keep the ISO IN pipeline fed */
      self->_sendFromRing();
    }
    /* RX_MODE: buf was allocated in s_get_recv_buf and freed in
     * s_data_recv; nothing to do here. */
  }

  /**
   * RX_MODE only: return nominal Q10.14 explicit feedback value.
   * Full-Speed: (samples_per_frame << 14) in 24 LSBs.
   * Replace with a hardware timer measurement for production use.
   */
  static uint32_t s_get_feedback(const struct device* dev, uint8_t terminal,
                                 void* user_data) {
    ARG_UNUSED(dev);
    ARG_UNUSED(terminal);
    auto* self = static_cast<USBAudioStream*>(user_data);
    uint32_t spf = self->cfg_.sample_rate / 1000U;
    return (spf << 14) & 0x00FFFFFFu;
  }
};

} /* namespace audio_tools */
