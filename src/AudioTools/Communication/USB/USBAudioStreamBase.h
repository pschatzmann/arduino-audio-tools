#pragma once

// See USBAudioDeviceBase.h: some cores #define abs(x), which breaks <chrono>
// (pulled in by <mutex>/<functional> on some toolchains).
#pragma push_macro("abs")
#undef abs

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#pragma pop_macro("abs")

#include "AudioLogger.h"
#include "AudioTools/CoreAudio/AudioTypes.h"
#include "AudioTools/CoreAudio/BaseStream.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "AudioTools/Communication/USB/USBAudioConfig.h"
#include "AudioTools/Communication/USB/USBAudioConstants.h"

namespace audio_tools {

/**
 * @brief Role-independent front end shared by USB Audio devices
 * (USBAudioDeviceBase, aliased as USBAudioStream) and USB Audio hosts
 * (USBAudioHostBase, aliased as USBAudioHostStream).
 *
 * It provides the identical public Arduino stream API for both roles:
 * write() sends audio to the USB peer, readBytes() receives audio from it,
 * plus volume/mute state, callbacks and the one-shot RX startup prime.
 * The USB stack specific parts are provided by the subclasses via the pure
 * virtual methods.
 *
 * @ingroup usb
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class USBAudioStreamBase : public AudioStream, public VolumeSupport {
 public:
  /** @brief Returns true if the IN endpoint is enabled. */
  inline bool isEpInEnabled() const { return config_.enable_ep_in; }

  /** @brief Returns true if the OUT endpoint is enabled. */
  inline bool isEpOutEnabled() const { return config_.enable_ep_out; }

  /// Provides the active configuration
  const USBAudioConfig& config() const { return config_; }

  // ── Volume / Mute API ───────────────────────────────────────────────────

  /// gets the volume for the master channel (channel 0)
  float volume() override { return volume(0); }

  /// sets the volume for the master channel (channel 0)
  bool setVolume(float volume) override { return setVolume(volume, 0); }

  /** @brief Returns the current volume for the given channel.
   *  @param channel  0 = master, 1..N = per-channel.
   *  @return Volume as a float in the range 0.0 (silence) to 1.0 (0 dB). */
  float volume(uint8_t channel) {
    return (channel < volume_.size()) ? volume_[channel] : 0.0f;
  }

  /** @brief Set the volume for a channel and notify the USB peer.
   *  @param vol      Volume as a float 0.0 (silence) to 1.0 (0 dB).
   *  @param channel  0 = master, 1..N = per-channel.
   *  @return true if the channel index was valid. */
  bool setVolume(float vol, uint8_t channel) {
    LOGW("setVolume %f channel: %d", vol, channel);
    if (channel >= volume_.size()) return false;
    volume_[channel] = vol;
    if (volume_cb_) volume_cb_(vol, channel);
    onVolumeChanged(channel);
    return true;
  }

  /** @brief Returns the current mute state for the given channel.
   *  @param channel  0 = master, 1..N = per-channel. */
  bool isMute(uint8_t channel = 0) const {
    return (channel < mute_.size()) ? mute_[channel] : false;
  }

  /** @brief Set the mute state for a channel and notify the USB peer.
   *  @param m        true = muted, false = unmuted.
   *  @param channel  0 = master, 1..N = per-channel.
   *  @return true if the channel index was valid. */
  bool setMute(bool m, uint8_t channel = 0) {
    LOGW("setMute %s channel: %d", m ? "true" : "false", channel);
    if (channel >= mute_.size()) return false;
    mute_[channel] = m;
    if (mute_cb_) mute_cb_(m, channel);
    onMuteChanged(channel);
    return true;
  }

  /** @brief Register a callback invoked when the volume changes.
   *  @param cb  Callback receiving (float volume, uint8_t channel). */
  void setVolumeCallback(std::function<void(float, uint8_t)> cb) {
    volume_cb_ = std::move(cb);
  }

  /** @brief Register a callback invoked when the mute state changes.
   *  @param cb  Callback receiving (bool muted, uint8_t channel). */
  void setMuteCallback(std::function<void(bool, uint8_t)> cb) {
    mute_cb_ = std::move(cb);
  }

  /** @brief Register a callback invoked when the sample rate changes.
   *  @param cb  Callback receiving the new rate in Hz. */
  void setSampleRateCallback(std::function<void(uint32_t)> cb) {
    sample_rate_cb_ = std::move(cb);
  }

  /** @brief Register a callback invoked when the streaming state changes
   *  (a streaming interface was opened or closed).
   *  @param cb  Callback receiving (bool active_tx, bool active_rx). */
  void setStreamingStateCallback(std::function<void(bool, bool)> cb) {
    streaming_state_cb_ = std::move(cb);
  }

  /** @brief Returns true if either the TX or RX stream is open. */
  bool isStreamingActive() const {
    return isStreamingActiveTx() || isStreamingActiveRx();
  }

  /** @brief Returns true if the stream used by write() is open. */
  virtual bool isStreamingActiveTx() const = 0;

  /** @brief Returns true if the stream used by readBytes() is open. */
  virtual bool isStreamingActiveRx() const = 0;

  /** @brief Returns true if the USB connection is established (device:
   *  enumerated by the host; host: an audio device is attached). */
  virtual bool mounted() = 0;

  /** @brief Send audio data to the USB peer.
   *  Silently discards data while the TX stream is not open so StreamCopy
   *  doesn't report write errors before streaming starts. */
  size_t write(const uint8_t* data, size_t len) {
    if (!is_started_) return 0;
    serviceUSB();

    // disregard data if the TX stream has not been opened (alt=0)
    if (!isStreamingActiveTx()) return len;

    // update the volume
    if (config_.volume_active) processVolume((uint8_t*)data, len);

    // Write all data, retrying if the buffer is full.  On single-core
    // platforms (RP2040), serviceUSB() drains the buffer by running
    // the USB stack task → xfer_cb.  On dual-core (ESP32), the USB task
    // drains independently and the RTOS buffer blocks internally.
    size_t written = 0;
    while (written < len) {
      int n = bufferTx().writeArray(data + written, len - written);
      written += n;
      if (written < len) {
        serviceUSB();                       // drain buffer to make space
        if (!isStreamingActiveTx()) break;  // stream stopped
      }
    }
    return written;
  }

  /** @brief Receive audio data from the USB peer.
   *  Until bufferRx() has filled past config.rx_start_fill_percent at least
   *  once after begin(), no data is returned. This is a one-shot startup
   *  prime only -- once reached, readBytes() keeps draining normally for the
   *  rest of the session and is not gated again by later underruns. */
  size_t readBytes(uint8_t* buffer, size_t bufsize) {
    if (!is_started_) return 0;
    serviceUSB();

    if (!rx_primed_) {
      uint8_t start_fill = config_.rx_start_fill_percent > 100
                               ? 100
                               : config_.rx_start_fill_percent;
      if (start_fill > 0 && bufferRx().levelPercent() < start_fill) return 0;
      rx_primed_ = true;
    }

    // get the data from the buffer
    size_t ret = bufferRx().readArray(buffer, bufsize);
    // upate the volume
    if (config_.volume_active) processVolume(buffer, ret);

    return ret;
  }

  /** @brief Bytes of received audio waiting in the RX buffer. */
  int available() override {
    if (!is_started_) return 0;
    return bufferRx().available();
  }

  /** @brief Bytes of free space in the TX buffer. */
  int availableForWrite() override {
    if (!is_started_) return 0;
    return bufferTx().availableForWrite();
  }

  /// Returns true when begin() has been called and the USB connection is
  /// established.
  operator bool() override { return is_started_ && mounted(); }

  /** @brief Returns the TX audio buffer.  Must be overridden by subclasses. */
  virtual BaseBuffer<uint8_t>& bufferTx() = 0;

  /** @brief Returns the RX audio buffer.  Must be overridden by subclasses. */
  virtual BaseBuffer<uint8_t>& bufferRx() = 0;

  /** @brief Process pending USB events on platforms where the application
   *  drives the stack. No-op where a dedicated task runs the stack. */
  virtual void serviceUSB() = 0;

  /// Returns the actual core on which the call is running (for RP2040)
  virtual int getActualCore() const { return -1; }

 protected:
  bool is_started_ = false;
  // true while a dedicated FreeRTOS task is running the USB stack task
  bool usb_task_active_ = false;
  int core = -1;
  bool rx_primed_ = false;
  bool is_active_ = false;

  // ── Volume / mute state (sized to channels+1 in begin()) ─────────────────
  std::vector<float> volume_;
  std::vector<bool> mute_;
  std::function<void(float, uint8_t)> volume_cb_;
  std::function<void(bool, uint8_t)> mute_cb_;
  std::function<void(uint32_t)> sample_rate_cb_;
  std::function<void(bool, bool)> streaming_state_cb_;
  USBAudioConfig config_;

  /// Called by setVolume() after the state was updated: propagate the change
  /// to the USB peer.
  virtual void onVolumeChanged(uint8_t /*channel*/) {}

  /// Called by setMute() after the state was updated: propagate the change
  /// to the USB peer.
  virtual void onMuteChanged(uint8_t /*channel*/) {}

  /** @brief Override in platform subclasses to start the USB stack. */
  virtual bool beginUSB() = 0;

  /** @brief Resize the platform audio buffers. */
  virtual void resizeBuffers() = 0;

  /// Set the USB audio configuration (use begin(cfg) instead).
  void setConfig(const USBAudioConfig& cfg) { config_ = cfg; }

  bool configChanged(const USBAudioConfig& n) { return config_ != n; }

  /// (Re)sizes the volume and mute state: master (index 0) + one per channel
  void setupVolumeState() {
    const size_t vol_sz = (size_t)config_.channels + 1;
    volume_.assign(vol_sz, 1.0f);
    mute_.assign(vol_sz, false);
  }

  /** @brief Process audio data for volume control. */
  void processVolume(uint8_t* data, size_t len) {
    switch (config_.bits_per_sample) {
      case 8:
        processVolume<int8_t>((int8_t*)data, len);
        break;
      case 16:
        processVolume<int16_t>((int16_t*)data, len / 2);
        break;
      case 24:
        processVolume<int24_3bytes_t>((int24_3bytes_t*)data, len / 3);
        break;
      case 32:
        processVolume<int32_t>((int32_t*)data, len / 4);
        break;
      default:
        // Unsupported bit depth; do nothing.
        break;
    }
  }

  /** @brief Returns the effective volume for a per-channel index (1-based).
   *  Combines master volume (index 0) with per-channel volume and mute. */
  float getVolumeExt(uint8_t channel) const {
    if (volume_.empty()) return 1.0f;
    if (mute_[0]) return 0.0f;  // master mute
    float master = volume_[0];
    if (channel >= volume_.size()) return master;  // no per-channel entry
    if (mute_[channel]) return 0.0f;               // per-channel mute
    return master * volume_[channel];
  }

  template <typename T>
  void processVolume(T* data, size_t sample_count) {
    uint8_t ch_count = config_.channels;
    for (size_t i = 0; i < sample_count; i++) {
      uint8_t ch = (uint8_t)(i % ch_count) + 1;  // 1-based per-channel index
      float vol = getVolumeExt(ch);
      data[i] = (T)(data[i] * vol);
    }
  }

  /** @brief Convert AudioTools volume (0.0–1.0) to UAC2 int16 (1/256 dB).
   *  0.0 maps to 0x8000 (silence), 1.0 maps to 0 (0 dB). */
  static constexpr int16_t kVolumeMinDb256 = -25600;  // -100 dB in 1/256 dB

  /** @brief Convert linear volume (0.0–1.0) to UAC2 int16 (1/256 dB).
   *  Linear mapping: 0.0 → -100 dB (min), 1.0 → 0 dB (max). */
  static int16_t floatToUac2(float vol) {
    if (vol <= 0.0f) return (int16_t)0x8000;
    if (vol >= 1.0f) return 0;
    return (int16_t)((1.0f - vol) * kVolumeMinDb256);
  }

  /** @brief Convert UAC2 int16 (1/256 dB) to linear volume (0.0–1.0).
   *  Linear mapping within the -100..0 dB range reported by GET_RANGE. */
  static float uac2ToFloat(int16_t v) {
    if (v == (int16_t)0x8000) return 0.0f;
    if (v >= 0) return 1.0f;
    if (v <= kVolumeMinDb256) return 0.0f;
    return 1.0f - (float)v / (float)kVolumeMinDb256;
  }

  static bool isValidBitsPerSample(uint8_t bps) {
    return bps == 16 || bps == 24 || bps == 32;
  }

  void notifyStreamingState() {
    if (streaming_state_cb_)
      streaming_state_cb_(isStreamingActiveTx(), isStreamingActiveRx());
  }
};

}  // namespace audio_tools
