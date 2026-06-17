#pragma once
#include "AudioTools/Concurrency/ITask.h"
#include "AudioTools/Concurrency/LockFree.h"
#include "AudioTools/CoreAudio/BaseStream.h"
#include "AudioTools/Sandbox/USB/USBAudioDevice.h"

namespace audio_tools {

/**
 * @brief USB Audio Class 2.0 stream — template parameterized on the device
 * type.
 *
 * @tparam Device  Platform-specific USBAudioDeviceBase subclass.
 *                 Use USBAudioDeviceTinyUSB for RP2040/arduino-pico and
 *                 USBAudioDeviceESP32 for ESP32-S2/S3.
 *
 * Type aliases are provided for convenience:
 * @code
 *   #include "AudioTools/Sandbox/USB/USBAudioStream.h"
 *   USBAudioStream usb(cfg);   // = USBAudioStream<USBAudioDeviceTinyUSB>
 *
 * @endcode
 *
 * Data flow:
 *  TX (device -> host, microphone):
 *    write() -> tx_queue_ -> tx_callback -> process() -> ep_in_ff -> USB xfer_cb
 *  RX (host -> device, speaker):
 *    USB xfer_cb -> ep_out_ff -> process() -> rx_callback -> rx_queue_ -> readBytes()
 *
 * Thread safety:
 *  rx_queue_ and tx_queue_ are QueueLockFree (MPMC lock-free) so process()
 *  and readBytes()/write() can run concurrently without a mutex.
 *
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <typename Device = USBAudioDevice>
class USBAudioStream : public AudioStream {
 public:
  USBAudioStream() = default;

  explicit USBAudioStream(ITask& task) { p_task = &task; }

  /// Construct and pre-load the config so the RP2040 descriptor callbacks
  /// return valid data before setup() runs.
  USBAudioStream(USBAudioConfig cfg) {
    cfg_ = cfg;
    AudioStream::setAudioInfo(cfg);
    dev_.setConfig(cfg_);
  }

  USBAudioStream(USBAudioConfig cfg, ITask& task) : p_task(&task) {
    cfg_ = cfg;
    AudioStream::setAudioInfo(cfg);
    dev_.setConfig(cfg_);
  }

  ~USBAudioStream() { end(); }

  /// Returns a config pre-filled for the requested direction.
  USBAudioConfig defaultConfig(RxTxMode mode = RXTX_MODE) {
    return dev_.defaultConfig(mode);
  }

  bool begin(USBAudioConfig cfg) {
    cfg_ = cfg;
    AudioStream::setAudioInfo(cfg);
    return begin();
  }

  bool begin() override {
    if (!cfg_) {
      LOGE("USBAudioStream: sample_rate/channels/bits_per_sample not configured");
      return false;
    }

    dev_.setRxCallback([this](const uint8_t* data, uint16_t len) {
      for (uint16_t i = 0; i < len; ++i) rx_queue_.enqueue(data[i]);
    });
    dev_.setTxCallback([this](uint8_t* data, uint16_t len) -> uint16_t {
      uint8_t b;
      uint16_t i = 0;
      while (i < len && tx_queue_.dequeue(b)) data[i++] = b;
      if (i < len) memset(data + i, 0, (size_t)(len - i));
      return len;
    });

    bool was_active = is_active_;
    is_active_ = dev_.begin(cfg_);  // no-op if already started with same config
    if (!is_active_) return false;

    const uint16_t pkt = dev_.audioPacketSize();
    rx_queue_.resize(pkt * cfg_.fifo_packets);
    tx_queue_.resize(pkt * cfg_.fifo_packets);

    if (!was_active && p_task) p_task->begin([this]() { process(); });
    return true;
  }

  void end() override {
    if (is_active_) {
      dev_.end();
      rx_queue_.clear();
      tx_queue_.clear();
      is_active_ = false;
    }
    if (p_task) p_task->end();
  }

  /// Reconfigure audio format on the fly; re-enumerates USB only if changed.
  void setAudioInfo(AudioInfo info) override {
    AudioStream::setAudioInfo(info);
    cfg_.sample_rate     = info.sample_rate;
    cfg_.channels        = info.channels;
    cfg_.bits_per_sample = info.bits_per_sample;
    if (is_active_) {
      dev_.reenumerateUSBOnChange(cfg_);
      const uint16_t pkt = dev_.audioPacketSize();
      rx_queue_.resize(pkt * cfg_.fifo_packets);
      tx_queue_.resize(pkt * cfg_.fifo_packets);
    }
  }

  /// Call regularly in loop() or via ITask to keep audio flowing.
  void process() {
    dev_.process();  // tud_task() called inside on TinyUSB platforms
    yield();
  }


  size_t write(const uint8_t* data, size_t len) override {
    if (!is_active_) return 0;
    size_t i = 0;
    while (i < len && tx_queue_.enqueue(data[i])) ++i;
    return i;
  }

  size_t readBytes(uint8_t* data, size_t len) override {
    if (!is_active_) return 0;
    size_t i = 0;
    uint8_t b;
    while (i < len && rx_queue_.dequeue(b)) data[i++] = b;
    return i;
  }

  int available() override { return is_active_ ? (int)rx_queue_.size() : 0; }

  int availableForWrite() override {
    return is_active_ ? (int)(tx_queue_.capacity() - tx_queue_.size()) : 0;
  }

  operator bool() override { return is_active_ && dev_.mounted(); }

 protected:
  Device dev_;
  USBAudioConfig cfg_;
  bool is_active_ = false;
  QueueLockFree<uint8_t> rx_queue_{1};
  QueueLockFree<uint8_t> tx_queue_{1};
  ITask* p_task = nullptr;
};

}  // namespace audio_tools
