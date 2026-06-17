#pragma once
#include "AudioTools/Concurrency/ITask.h"
#include "AudioTools/Concurrency/LockFree.h"
#include "AudioTools/CoreAudio/BaseStream.h"
#include "AudioTools/Sandbox/USB/USBAudioDevice.h"

namespace audio_tools {

/**
 * @brief USB Audio Class 2.0 stream implementing the AudioStream interface.
 *
 * Wraps USBAudioDevice (singleton) and adds application-side lock-free queues
 * so the USB isochronous timing is fully decoupled from the application's
 * read/write cadence.
 *
 * Data flow:
 *  TX (device → host, microphone):
 *    write() → tx_queue_ ← tx_callback ← process() ← ep_in_ff ← USB xfer_cb
 *  RX (host → device, speaker):
 *    USB xfer_cb → ep_out_ff → process() → rx_callback → rx_queue_ → readBytes()
 *
 * In linear-buffer mode the FIFO ↔ lin_buf copying is handled transparently
 * inside USBAudioDevice and is invisible here.
 *
 * Thread safety:
 *  rx_queue_ and tx_queue_ are QueueLockFree instances (SPSC lock-free) so
 *  process() (which may run in a dedicated ITask thread) and
 *  readBytes()/write() (called from the application) can safely run
 *  concurrently without any mutex.
 *
 * Usage:
 * @code
 *   USBAudioStream usb;
 *   auto cfg = usb.defaultConfig(RX_MODE);
 *   cfg.sample_rate = 48000; cfg.channels = 2; cfg.bits_per_sample = 16;
 *   usb.begin(cfg);
 *
 *   void loop() {
 *     tud_task();        // drives the TinyUSB stack
 *     usb.process();     // moves data between USB FIFOs and ring buffers
 *     uint8_t buf[512];
 *     usb.readBytes(buf, sizeof(buf));   // feed to DAC / I2S …
 *   }
 * @endcode
 *
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class USBAudioStream : public AudioStream {
 public:
  USBAudioStream() = default;

  USBAudioStream(ITask& task) { p_task = &task; }

  ~USBAudioStream() { end(); }

  /// Returns a config pre-filled for the requested direction.
  USBAudioConfig defaultConfig(RxTxMode mode = RXTX_MODE) {
    return USBAudioDevice::instance().defaultConfig(mode);
  }

  /**
   * @brief Start USB audio with an explicit config.
   *
   * AudioInfo fields already provided via setAudioInfo() are used as fallback
   * for any zero-valued fields in @p cfg.
   */
  bool begin(USBAudioConfig cfg) {
    AudioInfo base = audioInfo();
    if (cfg.sample_rate == 0) cfg.sample_rate = base.sample_rate;
    if (cfg.channels == 0) cfg.channels = base.channels;
    if (cfg.bits_per_sample == 0) cfg.bits_per_sample = base.bits_per_sample;
    cfg_ = cfg;
    AudioStream::setAudioInfo(cfg_);
    return startDevice();
  }

  /// Restart with the last config, refreshing AudioInfo from setAudioInfo().
  bool begin() override {
    AudioInfo base = audioInfo();
    if (base.sample_rate) cfg_.sample_rate = base.sample_rate;
    if (base.channels) cfg_.channels = base.channels;
    if (base.bits_per_sample) cfg_.bits_per_sample = base.bits_per_sample;
    return startDevice();
  }

  /// Stop audio streaming and clear queues and USB FIFOs.
  void end() override {
    if (is_active_) {
      USBAudioDevice::instance().end();
      rx_queue_.clear();
      tx_queue_.clear();
      is_active_ = false;
    }
    if (p_task) p_task->end();
  }

  /**
   * @brief Reconfigure audio parameters on the fly.
   *
   * When already active the device is restarted so the host sees the new
   * sample rate / channel count immediately.
   */
  void setAudioInfo(AudioInfo info) override {
    AudioStream::setAudioInfo(info);
    if (is_active_) {
      cfg_.sample_rate = info.sample_rate;
      cfg_.channels = info.channels;
      cfg_.bits_per_sample = info.bits_per_sample;
      startDevice();
    }
  }

  /**
   * @brief Move data between the USB FIFOs and the application queues.
   *
   * Call this regularly in the application loop() or in a dedicated task to
   * keep the USB audio streaming.
   */
  static void process() {
    tud_task();
    USBAudioDevice::instance().process();
    yield();
  }

  /**
   * @brief Write audio toward the USB host (microphone / capture path).
   * @return Bytes accepted into the TX queue.
   */
  size_t write(const uint8_t* data, size_t len) override {
    if (!is_active_) return 0;
    size_t i = 0;
    while (i < len && tx_queue_.enqueue(data[i])) ++i;
    return i;
  }

  /**
   * @brief Read audio received from the USB host (speaker / playback path).
   * @return Bytes copied from the RX queue.
   */
  size_t readBytes(uint8_t* data, size_t len) override {
    if (!is_active_) return 0;
    size_t i = 0;
    uint8_t b;
    while (i < len && rx_queue_.dequeue(b)) data[i++] = b;
    return i;
  }

  /// Bytes of received audio available in the RX queue.
  int available() override { return is_active_ ? (int)rx_queue_.size() : 0; }

  /// Free bytes remaining in the TX queue.
  int availableForWrite() override {
    return is_active_
               ? (int)(tx_queue_.capacity() - tx_queue_.size())
               : 0;
  }

  /// True when the USB host has enumerated the device and streaming is active.
  operator bool() override { return is_active_ && tud_mounted(); }

 protected:
  USBAudioConfig cfg_;
  bool is_active_ = false;
  // Capacity-1 placeholder; both queues are resized to the real packet count
  // inside startDevice() once audioPacketSize() is known.
  QueueLockFree<uint8_t> rx_queue_{1};
  QueueLockFree<uint8_t> tx_queue_{1};
  ITask* p_task = nullptr;

  bool startDevice() {
    if (!cfg_) {
      LOGE("USBAudioStream: sample_rate/channels/bits_per_sample not configured");
      return false;
    }

    auto& dev = USBAudioDevice::instance();

    // Register callbacks before begin() so they are in place when USB starts.
    dev.setRxCallback([this](const uint8_t* data, uint16_t len) {
      for (uint16_t i = 0; i < len; ++i) rx_queue_.enqueue(data[i]);
    });
    dev.setTxCallback([this](uint8_t* data, uint16_t len) -> uint16_t {
      uint8_t b;
      uint16_t i = 0;
      while (i < len && tx_queue_.dequeue(b)) data[i++] = b;
      if (i < len) memset(data + i, 0, (size_t)(len - i));
      return len;
    });

    is_active_ = dev.begin(cfg_);
    if (!is_active_) return false;

    // Size queues using the packet size the device just computed from the
    // config, keeping the two in sync via a single source of truth.
    // Note: USBAudioDevice maintains an equal-sized internal FIFO, so the
    // true end-to-end buffer depth is 2 × fifo_packets packets.
    // QueueLockFree rounds capacity up to the next power of 2 automatically.
    const uint16_t pkt = dev.audioPacketSize();
    rx_queue_.resize(pkt * cfg_.fifo_packets);
    tx_queue_.resize(pkt * cfg_.fifo_packets);

    if (p_task) p_task->begin([]() { process(); });

    return true;
  }
};

}  // namespace audio_tools
