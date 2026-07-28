#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <mutex>
#include <vector>

#include "AudioTools/CoreAudio/AudioTypes.h"
#include "AudioTools/CoreAudio/Buffers.h"

#ifdef ESP32
#ifndef ARDUINO_USB_MODE
#error This Microcontroller has no Native USB interface
#else
#if ARDUINO_USB_MODE == 1
#error This sketch should be used when USB is in OTG mode
#endif
#endif
#else
#if !defined(USE_TINYUSB) && !defined(ARDUINO_ARCH_STM32) && !defined(IS_ZEPHYR)
#error This Microcontroller has no Native USB interface
#endif
#endif

#include "AudioLogger.h"
#include "AudioTools/CoreAudio/BaseStream.h"
#include "AudioTools/Communication/USB/USBAudioBackend.h"
#include "USBAudio2DescriptorBuilder.h"

#define USB_DESCR_MAX_LEN 512

namespace audio_tools {

// ── UAC2 / USB 2.0 spec constants ───────────────────────────────────────────
// These are fixed byte values defined by the USB 2.0 and UAC2 specs, not
// TinyUSB API surface (TinyUSB's own headers just give them names). Owned
// here directly so USBAudioDeviceBase has zero dependency on any particular
// USB stack's headers.

// Descriptor types (USB 2.0 spec Table 9-5).
static constexpr uint8_t kUsbDescTypeInterface = 0x04;
static constexpr uint8_t kUsbDescTypeEndpoint = 0x05;
static constexpr uint8_t kUsbDescTypeCsInterface = 0x24;

// Audio class/subclass/protocol codes (UAC2 spec Appendix A).
static constexpr uint8_t kUsbClassAudio = 0x01;
static constexpr uint8_t kAudioSubclassControl = 0x01;
static constexpr uint8_t kAudioIntProtocolCodeV2 = 0x20;

// AC interface descriptor subtypes (UAC2 Table A-9).
static constexpr uint8_t kAudioCsAcInputTerminal = 0x02;
static constexpr uint8_t kAudioCsAcOutputTerminal = 0x03;
// AS interface descriptor subtypes (UAC2 Table A-11).
static constexpr uint8_t kAudioCsAsGeneral = 0x01;
static constexpr uint8_t kAudioCsAsFormatType = 0x02;
// Control request codes (UAC2 Table A-15).
static constexpr uint8_t kAudioCsReqCur = 0x01;
static constexpr uint8_t kAudioCsReqRange = 0x02;
// Clock Source control selectors (UAC2 Table A-18).
static constexpr uint8_t kAudioCsCtrlSamFreq = 0x01;
static constexpr uint8_t kAudioCsCtrlClkValid = 0x02;
// Feature Unit control selectors (UAC2 Table A-23).
static constexpr uint8_t AUDIO_FU_CTRL_MUTE = 0x01;
static constexpr uint8_t AUDIO_FU_CTRL_VOLUME = 0x02;
// Terminal type: USB Streaming (UAC2 Table 2-1).
static constexpr uint16_t kAudioTermTypeUsbStreaming = 0x0101;
// Interface Association Descriptor length — fixed 8 bytes.
static constexpr uint16_t kAudioDescIadLen = 8;

// USB control-transfer state-machine stages (universal to any USB stack's
// control-transfer handling, not TinyUSB-specific).
static constexpr uint8_t kControlStageSetup = 1;
static constexpr uint8_t kControlStageData = 2;

static inline uint8_t u16Low(uint16_t v) { return (uint8_t)(v & 0xFF); }
static inline uint8_t u16High(uint16_t v) { return (uint8_t)(v >> 8); }

/**
 * @brief USB Audio Device class for audio streaming over USB.
 *
 * This class implements a USB audio device, providing configuration, descriptor
 * management, endpoint setup, and control request handling for audio streaming
 * over USB. It supports multiple audio formats, feedback methods, and endpoint
 * configurations, and is designed for use with TinyUSB or native USB on
 * supported MCUs.
 * 
 * @author Phil Schatzmann
 * @ingroup usb
 * @ingroup io
 * @copyright GPLv3
 */
class USBAudioDeviceBase : public AudioStream, public VolumeSupport {
  /**
   * @brief Supported USB audio format types.
   *
   * Enumerates the audio format types supported by the USB audio device.
   */
  enum audio_format_type_t {
    AUDIO_FORMAT_TYPE_I,
    AUDIO_FORMAT_TYPE_II,
    AUDIO_FORMAT_TYPE_III,
  };

  /**
   * @brief Methods for USB audio feedback endpoint operation.
   *
   * Enumerates the available feedback methods for synchronizing audio streaming
   * between the device and the host.
   */
  enum audio_feedback_method_t {
    AUDIO_FEEDBACK_METHOD_DISABLED,
    AUDIO_FEEDBACK_METHOD_FREQUENCY_FIXED,
    AUDIO_FEEDBACK_METHOD_FREQUENCY_FLOAT,
    AUDIO_FEEDBACK_METHOD_FREQUENCY_POWER_OF_2,  // For driver internal use only
    AUDIO_FEEDBACK_METHOD_FIFO_COUNT
  };

  /**
   * @brief Internal structure representing a USB audio function instance.
   *
   * Holds endpoint addresses, descriptor pointers, buffer management, and
   * feedback parameters for a single audio function on the device.
   */
  struct audiod_function_t {
    uint8_t rhport;
    uint8_t const* p_desc;  // Pointer to Standard AC Interface Descriptor
    uint8_t ep_in;          // TX audio data EP.
    uint16_t ep_in_sz;      // Current size of TX EP
    uint8_t
        ep_in_as_intf_num;  // Standard AS Interface Descriptor number for IN
    uint8_t ep_out;         // RX audio data EP.
    uint16_t ep_out_sz;     // Current size of RX EP
    uint8_t
        ep_out_as_intf_num;  // Standard AS Interface Descriptor number for OUT
    uint8_t ep_fb;           // Feedback EP.
    uint8_t ep_int;          // Audio control interrupt EP.
    bool mounted;            // Device opened
    uint16_t desc_length;    // Length of audio function descriptor
    struct {
      uint32_t value;
      uint32_t min_value;
      uint32_t max_value;
      uint8_t frame_shift;
      uint8_t compute_method;
      bool format_correction;
      union {
        uint8_t power_of_2;
        float float_const;
        struct {
          uint32_t sample_freq;
          uint32_t mclk_freq;
        } fixed;
        struct {
          uint32_t nom_value;
          uint32_t fifo_lvl_avg;
          uint16_t fifo_lvl_thr;
          uint16_t rate_const[2];
        } fifo_count;
      } compute;
    } feedback;
    uint32_t sample_rate_tx;
    uint16_t packet_sz_tx[3];
    uint8_t bclock_id_tx;
    uint8_t interval_tx;
    audio_format_type_t format_type_tx;
    uint8_t n_channels_tx;
    uint8_t n_bytes_per_sample_tx;
    // Fractional sample accumulator for IN-endpoint flow control. Carries the
    // sub-frame remainder (e.g. the 0.1 sample/frame of 44100 Hz) so the
    // long-term average packet size matches the real sample rate.
    uint32_t tx_sample_acc;
    // Cached OUT endpoint descriptor (populated by audiod_open()'s descriptor
    // scan) so closeEpOut() can re-activate the endpoint -- resetting its
    // busy/claimed state for the next SET_INTERFACE -- without needing to
    // re-walk the descriptor. See closeEpOut() for why this is needed.
    UsbEndpointDescriptorView ep_out_view;
    // From this point, data is not cleared by bus reset
    uint8_t ctrl_buf_sz;
    std::vector<uint8_t> ctrl_buf;
    std::vector<uint8_t> alt_setting;
    std::vector<uint8_t> lin_buf_out;
    std::vector<uint8_t> lin_buf_in;
    std::vector<uint32_t> fb_buf;
  };

  /**
   * @brief Parameters for audio feedback endpoint configuration.
   *
   * Used to configure the feedback method and associated sample/clock
   * frequencies for USB audio feedback endpoints.
   */
  struct audio_feedback_params_t {
    uint8_t method;
    uint32_t sample_freq;  //  sample frequency in Hz

    union {
      struct {
        uint32_t mclk_freq;  // Main clock frequency in Hz i.e. master clock to
                             // which sample clock is based on
      } frequency;
    };
  };

 public:
  /// Default Constructor
  USBAudioDeviceBase() { s_active_ = this; }

  /// Constructor which provides configuration at construction time
  USBAudioDeviceBase(USBAudioConfig cfg) : USBAudioDeviceBase() {
    config_ = cfg;
  }

  /** @brief Returns a default configuration pre-filled for the requested
   *         direction (RX_MODE, TX_MODE, or RXTX_MODE).
   *  Use TX if you write and rx if you want to read audio data. RXTX is for
   * full duplex.
   *  @param mode  Audio direction. */
  USBAudioConfig defaultConfig(RxTxMode mode = RXTX_MODE) {
    USBAudioConfig cfg;
    switch (mode) {
      case RX_MODE:
        cfg.enable_ep_out = true;
        cfg.enable_ep_in = false;
        break;
      case TX_MODE:
        cfg.enable_ep_out = false;
        cfg.enable_ep_in = true;
        break;
      case RXTX_MODE:
        cfg.enable_ep_out = true;
        cfg.enable_ep_in = true;
        break;
      default:
        break;
    }
    return cfg;
  }

  /** @brief Change the sample rate and notify the host. */
  void setAudioInfo(AudioInfo info) override {
    if (!isValidBitsPerSample(info.bits_per_sample)) {
      LOGE("Unsupported bits_per_sample: %d (must be 16, 24, or 32)",
           info.bits_per_sample);
      return;
    }
    // full flexibility when not started yet
    if (!is_started_) {
      config_.sample_rate = info.sample_rate;
      config_.channels = info.channels;
      config_.bits_per_sample = info.bits_per_sample;
      return;
    }

    // when started only sample rate can be changed on the fly
    config_.sample_rate = info.sample_rate;
    if (config_.channels != info.channels) {
      LOGE(
          "Could not change channel count from %d to %d: channel count is "
          "fixed at startup!",
          config_.channels, info.channels);
    }
    if (config_.bits_per_sample != info.bits_per_sample) {
      LOGE(
          "Could not change bits per sample from %d to %d: bits per sample is "
          "fixed at startup!",
          config_.bits_per_sample, info.bits_per_sample);
    }
    // notifiy subscribed entities
    AudioStream::setAudioInfo(info);
    // notify host about sample rate change via control request callback;
    setSampleRate(config_.sample_rate);
  }

  /** @brief Apply a config and start the USB audio device.
   *
   *  Before the first begin() all fields may be changed freely.
   *  Once started, only the sample rate can change at runtime (via
   *  setSampleRate()); descriptor-level fields like channels, bit depth,
   *  and endpoint addresses are fixed by the USB enumeration.
   *  @param cfg  Audio configuration.
   *  @return true on success. */
  bool begin(const USBAudioConfig& cfg) {
    if (!is_started_) {
      config_ = cfg;
    } else if (!configChanged(cfg)) {
      return true;  // already running with same config
    } else {
      config_.sample_rate = cfg.sample_rate;
    }
    return begin();
  }

  /** @brief (Re-)start the USB audio device with the current config.
   *
   *  Safe to call more than once: descriptor building, FIFO allocation,
   *  and USB stack startup only run on the first call.  Subsequent calls
   *  just push the current volume/mute/sample-rate state to the host.
   *  @return true on success. */
  bool begin() {
    if (!is_started_) {
      if (!isValidBitsPerSample(config_.bits_per_sample)) {
        LOGE("Unsupported bits_per_sample: %d (must be 16, 24, or 32)",
             config_.bits_per_sample);
        return false;
      }

      // Resize platform buffers — virtual so each platform uses the right API.
      resizeBuffers();

      const int n = getAudioCount();
      // 192 bytes needed for multi-rate RANGE (2+14*12=170); 64 suffices for single rate
      uint16_t cb_sz = config_.enable_multi_sample_rate ? 192u : 64u;
      ctrl_buf_sz_.assign(n, cb_sz);

      uint8_t desc[USB_DESCR_MAX_LEN];
      uint16_t desc_len = descr_builder.buildFullDescriptor(desc);
      desc_len_.assign(n, desc_len);

      // master (index 0) + one per channel
      const size_t vol_sz = (size_t)config_.channels + 1;
      volume_.assign(vol_sz, 1.0f);
      mute_.assign(vol_sz, false);


      audiod_init();

      if (!beginUSB()) {
        LOGE("beginUSB failed");
        return false;
      }
      is_started_ = true;
    }

    // Push current state to the host.  sendInterruptNotification()
    // is a no-op when not yet mounted, so this is harmless on first boot.
    setSampleRate(config_.sample_rate);
    for (uint8_t ch = 0; ch < (uint8_t)volume_.size(); ch++) {
      setVolume(volume(ch), ch);
      setMute(isMute(ch), ch);
    }
    is_active_ = true;
    return true;
  }
  /**
   * @brief Returns the most-recently-constructed instance (base or subclass).
   *
   * Set in the constructor via s_active_, so usbd_app_driver_get_cb() and the
   * static process() trampoline can reach the right object without a singleton.
   */
  static USBAudioDeviceBase& activeInstance() { return *s_active_; }

  /** @brief Returns true if the IN endpoint is enabled. */
  inline bool isEpInEnabled() const { return config_.enable_ep_in; }

  /** @brief Returns true if the OUT endpoint is enabled. */
  inline bool isEpOutEnabled() const { return config_.enable_ep_out; }

  /** @brief Returns true if the feedback endpoint is enabled.
   *  Only meaningful in pure RX (OUT-only) mode: with an IN endpoint present
   *  the host uses the TX stream as implicit feedback instead. */
  bool isFeedbackEpEnabled() const { return descr_builder.enableFeedbackEp(); }

  /** @brief Returns true if IN endpoint flow control is enabled. When on, the
   *  per-frame isochronous packet size is varied so non-integer sample-per-
   *  frame rates (e.g. 44100 Hz) are delivered at the exact average rate. */
  bool isEpInFlowControlEnabled() const {
    return config_.enable_ep_in_flow_control;
  }

  // ── Volume / Mute / Sample-rate API ─────────────────────────────────────

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

  /** @brief Set the volume for a channel and notify the host.
   *  @param vol      Volume as a float 0.0 (silence) to 1.0 (0 dB).
   *  @param channel  0 = master, 1..N = per-channel.
   *  @return true if the channel index was valid. */
  bool setVolume(float vol, uint8_t channel) {
    LOGW("setVolume %f channel: %d", vol, channel);
    if (channel >= volume_.size()) return false;
    volume_[channel] = vol;
    if (volume_cb_) volume_cb_(vol, channel);
    sendInterruptNotification(AUDIO_FU_CTRL_VOLUME, channel,
                              USBAudio2DescriptorBuilder::ENTITY_FU1);
    return true;
  }

  /** @brief Returns the current mute state for the given channel.
   *  @param channel  0 = master, 1..N = per-channel. */
  bool isMute(uint8_t channel = 0) const {
    return (channel < mute_.size()) ? mute_[channel] : false;
  }

  /** @brief Set the mute state for a channel and notify the host.
   *  @param m        true = muted, false = unmuted.
   *  @param channel  0 = master, 1..N = per-channel.
   *  @return true if the channel index was valid. */
  bool setMute(bool m, uint8_t channel = 0) {
    LOGW("setMute %s channel: %d", m ? "true" : "false", channel);
    if (channel >= mute_.size()) return false;
    mute_[channel] = m;
    if (mute_cb_) mute_cb_(m, channel);
    sendInterruptNotification(AUDIO_FU_CTRL_MUTE, channel,
                              USBAudio2DescriptorBuilder::ENTITY_FU1);
    return true;
  }

  /** @brief Register a callback invoked when the host (or device) changes
   *         the volume.
   *  @param cb  Callback receiving (float volume, uint8_t channel). */
  void setVolumeCallback(std::function<void(float, uint8_t)> cb) {
    volume_cb_ = std::move(cb);
  }

  /** @brief Register a callback invoked when the host (or device) changes
   *         the mute state.
   *  @param cb  Callback receiving (bool muted, uint8_t channel). */
  void setMuteCallback(std::function<void(bool, uint8_t)> cb) {
    mute_cb_ = std::move(cb);
  }

  /** @brief Register a callback invoked when the host (or device) changes
   *         the sample rate.
   *  @param cb  Callback receiving the new rate in Hz. */
  void setSampleRateCallback(std::function<void(uint32_t)> cb) {
    sample_rate_cb_ = std::move(cb);
  }

  /** @brief Register a callback invoked when the streaming state changes.
   *  Fires when the host opens or closes a streaming interface (SET_INTERFACE
   *  alt=1 / alt=0).
   *  @param cb  Callback receiving (bool active_tx, bool active_rx). */
  void setStreamingStateCallback(std::function<void(bool, bool)> cb) {
    streaming_state_cb_ = std::move(cb);
  }

  /** @brief Returns true if either IN or OUT streaming endpoint is open. */
  bool isStreamingActive() const {
    return isStreamingActiveTx() || isStreamingActiveRx();
  }

  /** @brief Returns true if the host has opened the IN (capture) stream. */
  bool isStreamingActiveTx() const {
    for (const auto& fct : audiod_fct_) {
      if (fct.ep_in != 0) return true;
    }
    return false;
  }

  /** @brief Returns true if the host has opened the OUT (playback) stream. */
  bool isStreamingActiveRx() const {
    for (const auto& fct : audiod_fct_) {
      if (fct.ep_out != 0) return true;
    }
    return false;
  }
  /** @brief Returns true if the interrupt endpoint is enabled. */
  bool isInterruptEpEnabled() const { return config_.enable_interrupt_ep; }

  /** @brief Returns the number of audio functions (always 1). */
  uint8_t getAudioCount() const { return 1; }

  /** @brief Returns true if the device is mounted by the USB host. */
  bool mounted() { return backend().mounted(); }

  /**
   * @brief Register a callback for GET requests on the interface.
   * @param cb Callback function.
   */
  void setGetReqItfCallback(std::function<bool(USBAudioDeviceBase*, uint8_t,
                                               UsbSetupPacket const&)>
                                cb) {
    get_req_itf_cb_ = cb;
  }

  /**
   * @brief Register a callback for GET requests on an endpoint.
   * @param cb Callback function.
   */
  void setGetReqEpCallback(std::function<bool(USBAudioDeviceBase*, uint8_t,
                                              UsbSetupPacket const&)>
                               cb) {
    get_req_ep_cb_ = cb;
  }

  /**
   * @brief Register a callback for feedback done events.
   * @param cb Callback function.
   */
  void setFbDoneCallback(std::function<void(USBAudioDeviceBase*, uint8_t)> cb) {
    fb_done_cb_ = cb;
  }

  /**
   * @brief Register a callback for interrupt done events.
   * @param cb Callback function.
   */
  void setIntDoneCallback(
      std::function<void(USBAudioDeviceBase*, uint8_t)> cb) {
    int_done_cb_ = cb;
  }

  /**
   * @brief Register a callback for TX done events.
   * @param cb Callback function.
   */
  void setTxDoneCallback(
      std::function<bool(USBAudioDeviceBase*, uint8_t, audiod_function_t*)>
          cb) {
    tx_done_cb_ = cb;
  }

  /**
   * @brief Register a callback for RX done events.
   * @param cb Callback function.
   */
  void setRxDoneCallback(std::function<bool(USBAudioDeviceBase*, uint8_t,
                                            audiod_function_t*, uint16_t)>
                             cb) {
    rx_done_cb_ = cb;
  }

  /**
   * @brief Register a callback for entity requests.
   * @param cb Callback function.
   */
  void setReqEntityCallback(
      std::function<bool(USBAudioDeviceBase*, uint8_t)> cb) {
    req_entity_cb_ = cb;
  }

  /**
   * @brief Register a callback for interface set requests.
   * @param cb Callback function.
   */
  void setTudAudioSetItfCallback(
      std::function<bool(USBAudioDeviceBase*, uint8_t,
                         UsbSetupPacket const&)>
          cb) {
    tud_audio_set_itf_cb_ = cb;
  }

  /**
   * @brief Register a callback for entity set requests.
   * @param cb Callback function.
   */
  void setReqEntityCallback(
      std::function<bool(USBAudioDeviceBase*, uint8_t,
                         UsbSetupPacket const&, uint8_t*)>
          cb) {
    tud_audio_set_req_entity_cb_ = cb;
  }

  /**
   * @brief Register a callback for interface set requests.
   * @param cb Callback function.
   */
  void setReqItfCallback(
      std::function<bool(USBAudioDeviceBase*, uint8_t,
                         UsbSetupPacket const&, uint8_t*)>
          cb) {
    tud_audio_set_req_itf_cb_ = cb;
  }

  /**
   * @brief Register a callback for endpoint set requests.
   * @param cb Callback function.
   */
  void setReqEpCallback(
      std::function<bool(USBAudioDeviceBase*, uint8_t,
                         UsbSetupPacket const&, uint8_t*)>
          cb) {
    tud_audio_set_req_ep_cb_ = cb;
  }

  /**
   * @brief Register a callback for interface close endpoint events.
   * @param cb Callback function.
   */
  void setItfCloseEpCallback(std::function<bool(USBAudioDeviceBase*, uint8_t,
                                                UsbSetupPacket const&)>
                                 cb) {
    tud_audio_set_itf_close_EP_cb_ = cb;
  }

  /**
   * @brief Register a callback for audio feedback parameter events.
   * @param cb Callback function.
   */
  void setAudioFeedbackParamsCallback(
      std::function<void(USBAudioDeviceBase*, uint8_t, uint8_t,
                         audio_feedback_params_t*)>
          cb) {
    tud_audio_feedback_params_cb_ = cb;
  }

  /**
   * @brief Register a callback for audio feedback format correction events.
   * @param cb Callback function.
   */
  void setAudioFeedbackFormatCorrectionCallback(
      std::function<bool(USBAudioDeviceBase*, uint8_t)> cb) {
    tud_audio_feedback_format_correction_cb_ = cb;
  }

  /** @brief Send audio data to the host (device → host, microphone/capture).
   *  Silently discards data when the host has not opened the capture device
   *  (alt=0) so StreamCopy doesn't report write errors before recording. */
  size_t write(const uint8_t* data, size_t len) {
    if (!is_started_) return 0;
    serviceTinyUSB();

    // disregard data if the host has not opened the capture device (alt=0)
    if (!isStreamingActiveTx()) return len;

    // update the volume
    if (config_.volume_active) processVolume((uint8_t*)data, len);

    // Write all data, retrying if the buffer is full.  On single-core
    // platforms (RP2040), serviceTinyUSB() drains the buffer by running
    // tud_task() → xfer_cb.  On dual-core (ESP32), the USB task drains
    // independently and the SynchronizedNBufferRTOS blocks internally.
    size_t written = 0;
    while (written < len) {
      int n = bufferTx().writeArray(data + written, len - written);
      written += n;
      if (written < len) {
        serviceTinyUSB();  // drain buffer to make space
        if (!isStreamingActiveTx()) break;  // host stopped
      }
    }
    return written;
  }

  /** @brief Receive audio data from the host (host → device, speaker/playback).
   */
  size_t readBytes(uint8_t* buffer, size_t bufsize) {
    if (!is_started_) return 0;
    serviceTinyUSB();
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

  /// Returns true when begin() has been called and the USB host has mounted the device.
  operator bool() override { return is_started_ && mounted(); }

  /** @brief Stop audio streaming and clear buffers. Does not disconnect USB. */
  void end() {
    for (auto& audio : audiod_fct_) {
      std::fill(audio.lin_buf_in.begin(), audio.lin_buf_in.end(), 0);
      std::fill(audio.lin_buf_out.begin(), audio.lin_buf_out.end(), 0);
    }
    bufferTx().reset();
    bufferRx().reset();
    is_started_ = false;
  }

  /** @brief One isochronous USB packet size in bytes (same formula as the
   * descriptor builder). */
  uint16_t audioPacketSize() const { return packetSize(); }

  /**
   * @brief Returns the audio-function descriptor block for use in
   *        tud_descriptor_configuration_cb().
   *
   * Call begin() first so that the config is known.  The returned pointer
   * points to an internal static buffer that is valid for the lifetime of the
   * program; copy it into your configuration descriptor buffer.
   *
   * @param[out] len  Total byte count of the returned block.
   * @return Pointer to the audio function descriptor bytes.
   */
  uint16_t getDescriptor(uint8_t* desc) {
    active_config_ = config_;  // save active config
    return descr_builder.buildFullDescriptor(desc);
  }

  /**
   * @brief Total number of USB interfaces claimed by the audio function
   *        (1 AC + 1 or 2 AS), for use in the bNumInterfaces field of the
   *        configuration descriptor.
   */
  uint8_t numInterfaces() const {
    return (uint8_t)(1 + (config_.enable_ep_out ? 1 : 0) +
                     (config_.enable_ep_in ? 1 : 0));
  }

  /// True if the initial isochronous IN transfer was armed successfully.
  bool isTxXferArmed() const { return tx_xfer_armed_; }

  /// Number of times audiod_xfer_cb fired for the IN endpoint.
  uint32_t getTxXferCount() const { return xfer_cb_tx_count_; }
  /// Number of times audiod_xfer_cb fired for the OUT endpoint.
  uint32_t getRxXferCount() const { return xfer_cb_rx_count_; }
  /// Total bytes received from host via OUT endpoint.
  uint32_t getRxTotalBytes() const { return rx_total_bytes_; }
  /// Total bytes read from ep_in_ff by xfer_cb (should grow at ~176KB/s for
  /// 44100Hz stereo 16-bit).
  uint32_t getTxFifoReadTotal() const { return tx_fifo_read_total_; }

  /// Last frame_bytes computed by flow control (should be ~176-180 for
  /// 44100Hz).
  uint16_t getTxFrameBytesLast() const { return tx_frame_bytes_last_; }
  /// xferred_bytes from the previous completed transfer (what the DCD actually
  /// sent).
  uint32_t getTxXferredLast() const { return tx_xferred_last_; }
  /// TX sample rate parsed from the descriptor (must be non-zero for flow control).
  uint32_t getTxSampleRate() const {
    return audiod_fct_.empty() ? 0 : audiod_fct_[0].sample_rate_tx;
  }
  /// TX channel count parsed from the descriptor.
  uint8_t getTxChannels() const {
    return audiod_fct_.empty() ? 0 : audiod_fct_[0].n_channels_tx;
  }
  /// TX bytes per sample parsed from the descriptor.
  uint8_t getTxBytesPerSample() const {
    return audiod_fct_.empty() ? 0 : audiod_fct_[0].n_bytes_per_sample_tx;
  }
  /// TX isochronous interval (bInterval) parsed from the descriptor.
  uint8_t getTxInterval() const {
    return audiod_fct_.empty() ? 0 : audiod_fct_[0].interval_tx;
  }

 protected:
  bool is_started_ = false;
  bool usb_task_active_ = false;  // true while a dedicated tud_task() FreeRTOS task is running
  bool tx_xfer_armed_ = false;
  volatile uint32_t xfer_cb_tx_count_ = 0;
  volatile uint32_t tx_fifo_read_total_ = 0;
  volatile uint32_t xfer_cb_rx_count_ = 0;
  volatile uint32_t rx_total_bytes_ = 0;
  volatile uint16_t tx_frame_bytes_last_ = 0;
  volatile uint32_t tx_xferred_last_ = 0;

  bool is_active_ = false;
  // ── Volume / mute state (sized to channels+1 in begin()) ─────────────────
  std::vector<float> volume_;
  std::vector<bool> mute_;
  std::function<void(float, uint8_t)> volume_cb_;
  std::function<void(bool, uint8_t)> mute_cb_;
  std::function<void(uint32_t)> sample_rate_cb_;
  std::function<void(bool, bool)> streaming_state_cb_;
  USBAudioConfig config_;
  USBAudioConfig active_config_;
  USBAudio2DescriptorBuilder descr_builder{config_};
  std::function<void(USBAudioDeviceBase*, uint8_t rhport)> int_done_cb_;
  std::function<bool(USBAudioDeviceBase*, uint8_t rhport, audiod_function_t*)>
      tx_done_cb_;
  std::function<bool(USBAudioDeviceBase*, uint8_t rhport, audiod_function_t*,
                     uint16_t xferred_bytes)>
      rx_done_cb_;
  // Callback for interface GET requests
  std::function<bool(USBAudioDeviceBase*, uint8_t rhport,
                     UsbSetupPacket const&)>
      get_req_itf_cb_;
  // Callback for endpoint GET requests
  std::function<bool(USBAudioDeviceBase*, uint8_t rhport,
                     UsbSetupPacket const&)>
      get_req_ep_cb_;

  // Callback for feedback done event
  std::function<void(USBAudioDeviceBase*, uint8_t func_id)> fb_done_cb_;
  std::function<bool(USBAudioDeviceBase*, uint8_t func_id)> req_entity_cb_;
  std::function<bool(USBAudioDeviceBase*, uint8_t rhport,
                     UsbSetupPacket const& p_request)>
      tud_audio_set_itf_cb_;
  std::function<bool(USBAudioDeviceBase*, uint8_t rhport,
                     UsbSetupPacket const& p_request, uint8_t* pBuff)>
      tud_audio_set_req_entity_cb_;

  std::function<bool(USBAudioDeviceBase*, uint8_t rhport,
                     UsbSetupPacket const& p_request, uint8_t* pBuff)>
      tud_audio_set_req_itf_cb_;

  std::function<bool(USBAudioDeviceBase*, uint8_t rhport,
                     UsbSetupPacket const& p_request, uint8_t* pBuff)>
      tud_audio_set_req_ep_cb_;

  std::function<bool(USBAudioDeviceBase*, uint8_t rhport,
                     UsbSetupPacket const& p_request)>
      tud_audio_set_itf_close_EP_cb_;

  std::function<void(USBAudioDeviceBase*, uint8_t func_id, uint8_t alt_itf,
                     audio_feedback_params_t* feedback_param)>
      tud_audio_feedback_params_cb_;

  std::function<bool(USBAudioDeviceBase*, uint8_t func_id)>
      tud_audio_feedback_format_correction_cb_;
  uint8_t int_notify_buf_[6] = {};

  // calculate!
  std::vector<uint16_t> desc_len_;
  // 64
  std::vector<uint16_t> ctrl_buf_sz_;

  std::vector<audiod_function_t> audiod_fct_;

  // s_active_ lets the class-driver trampolines and the static process()
  // trampoline reach the last-constructed instance without a singleton.
  inline static USBAudioDeviceBase* s_active_ = nullptr;

  /// Set the USB audio configuration (use begin(cfg) instead).
  void setConfig(const USBAudioConfig& cfg) { config_ = cfg; }

  /** @brief Process pending USB events on platforms where the application
   *         drives the stack (RP2040, STM32, ...). Skipped when a dedicated
   *         FreeRTOS task already calls tud_task() continuously (see
   *         usb_task_active_) - tud_task() is not re-entrant; calling it
   *         from two contexts concurrently corrupts TinyUSB's internal
   *         event queue and causes random hangs.
   *
   *         ESP32 (see USBAudioDeviceESP32) overrides this to an empty
   *         no-op, since its own FreeRTOS task already calls tud_task()
   *         continuously. TinyUSB-based platforms (USBAudioDeviceTinyUSB)
   *         override this to pump tud_task() directly -- driving the USB
   *         stack's event loop is inherently stack-specific and has no
   *         equivalent on a native-HAL backend, so it is not part of
   *         USBAudioBackend. */
  virtual void serviceTinyUSB() = 0;

  /** @brief Returns the backend used for all raw USB-stack calls (TinyUSB
   *  today, or a future native-HAL backend). Must be overridden by every
   *  platform subclass; the returned reference must outlive this object. */
  virtual USBAudioBackend& backend() = 0;

  /** @brief Returns the TX audio buffer.  Must be overridden by subclasses. */
  virtual BaseBuffer<uint8_t>& bufferTx() = 0;

  /** @brief Returns the RX audio buffer.  Must be overridden by subclasses. */
  virtual BaseBuffer<uint8_t>& bufferRx() = 0;

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

  /** @brief Device-initiated sample rate change.
   *
   *  Updates the internal config, fires the sample-rate callback, and
   *  notifies the host via the AC interrupt endpoint (Clock Source
   *  SAM_FREQ CUR).  The host will typically re-issue SET_INTERFACE to
   *  restart streaming at the new rate.
   *  @param rate  New sample rate in Hz (should be one of the 14 rates
   *               advertised in the Clock Source GET_RANGE response). */
  void setSampleRate(uint32_t rate) {
    bool rate_updated = rate != config_.sample_rate;
    config_.sample_rate = rate;
    LOGW("Sample rate changed to %u Hz", rate);
    if (rate_updated) {
      notifyAudioChange(config_);
      resizeBuffers();
    }
    for (auto& fct : audiod_fct_) fct.sample_rate_tx = rate;
    if (sample_rate_cb_) sample_rate_cb_(rate);
    sendInterruptNotification(kAudioCsCtrlSamFreq, 0,
                              USBAudio2DescriptorBuilder::ENTITY_CLOCK);
  }

  /** @brief Override in platform subclasses to register descriptors and start
   *         the USB host-controller stack (e.g. USB.begin() on ESP32).
   *         Called at the end of begin(cfg, info).  The base no-op is correct
   *         for RP2040 where TinyUSB is started by the system before setup().*/
  virtual bool beginUSB() = 0;

  /** @brief Resize the platform audio buffers.
   *  Both platforms use NBuffer-style block pools:
   *  block size = max USB packet, block count = fifo_packets.
   *  ESP32: SynchronizedNBufferRTOS (FreeRTOS queues, cross-core safe).
   *  RP2040: NBuffer (single-core, no synchronization needed). */
  virtual void resizeBuffers() = 0;

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

  /** @brief Returns true if the given entity ID is a Feature Unit (FU1 or FU2).
   */
  bool isFeatureUnit(uint8_t id) const {
    return id == USBAudio2DescriptorBuilder::ENTITY_FU1 ||
           id == USBAudio2DescriptorBuilder::ENTITY_FU2;
  }

  /** @brief Send a UAC2 status/change notification via the AC interrupt EP.
   *
   *  6-byte message: bInfo(1) bAttribute(1) wValue(2) wIndex(2).
   *  The host will re-query the affected control with GET_CUR.
   *  @param ctrlSel   Control selector (e.g. AUDIO_FU_CTRL_VOLUME).
   *  @param channel   Channel number (0 = master).
   *  @param entityID  Target entity (Feature Unit or Clock Source). */
  void sendInterruptNotification(uint8_t ctrlSel, uint8_t channel,
                                 uint8_t entityID) {
    if (!backend().mounted()) return;
    for (uint8_t i = 0; i < (uint8_t)audiod_fct_.size(); i++) {
      if (audiod_fct_[i].ep_int == 0) continue;
      int_notify_buf_[0] = 0x00;                // bInfo: interface, not vendor
      int_notify_buf_[1] = kAudioCsReqCur;      // bAttribute: CUR changed
      int_notify_buf_[2] = channel;             // wValue low = CN
      int_notify_buf_[3] = ctrlSel;             // wValue high = CS
      int_notify_buf_[4] = config_.itf_num_ac;  // wIndex low = interface
      int_notify_buf_[5] = entityID;            // wIndex high = entity ID
      (void)backend().transfer(0, audiod_fct_[i].ep_int, int_notify_buf_, 6);
      break;
    }
  }

  bool configChanged(const USBAudioConfig& n) { return config_ != n; }

  // Returns the control buffer size for a given function number
  uint16_t getCtrlBufSz(uint8_t fn) const {
    return (fn < ctrl_buf_sz_.size()) ? ctrl_buf_sz_[fn] : 64;
  }

  // Returns the descriptor length for a given function number
  uint16_t getDescLen(uint8_t fn) const {
    return (fn < desc_len_.size()) ? desc_len_[fn] : 0;
  }

  static bool isValidBitsPerSample(uint8_t bps) {
    return bps == 16 || bps == 24 || bps == 32;
  }

  void notifyStreamingState() {
    if (streaming_state_cb_)
      streaming_state_cb_(isStreamingActiveTx(), isStreamingActiveRx());
  }

  // Max Bytes for one 1 ms isochronous USB packet.
  uint16_t packetSize() const { return descr_builder.calcMaxPacketSize(); }

  // Returns the reset size for audiod_function_t up to and including
  // ctrl_buf_sz
  static constexpr size_t getResetSize() {
    return offsetof(audiod_function_t, ctrl_buf_sz) +
           sizeof(((audiod_function_t*)0)->ctrl_buf_sz);
  }

  // Called by audiod_sof_isr() at the feedback interval.
  // Computes the current feedback value then claims the EP and sends it.
  void tud_audio_feedback_interval_isr(uint8_t func_id,
                                       uint32_t /*frame_count*/,
                                       uint8_t frame_shift) {
    audiod_function_t* audio = &audiod_fct_[func_id];

    switch (audio->feedback.compute_method) {
      case AUDIO_FEEDBACK_METHOD_FIFO_COUNT: {
        // In linear-buffer mode, audio data flows lin_buf_out → bufferRx(),
        // completely bypassing ep_out_ff.  Reading ep_out_ff would always
        // return 0, driving the feedback to min_value and causing the host
        // to gradually reduce its send rate until the buffer drains (audible
        // periodic drop every 5-10 s).  Use the platform RX ring buffer level.
        uint32_t ff_count = (uint32_t)bufferRx().available();
        // Exponential weighted average keeps the level estimate stable
        audio->feedback.compute.fifo_count.fifo_lvl_avg =
            audio->feedback.compute.fifo_count.fifo_lvl_avg -
            (audio->feedback.compute.fifo_count.fifo_lvl_avg >> 8) +
            ((uint32_t)ff_count << 8);
        uint32_t avg = audio->feedback.compute.fifo_count.fifo_lvl_avg >> 8;
        uint32_t thr = audio->feedback.compute.fifo_count.fifo_lvl_thr;
        uint32_t nom = audio->feedback.compute.fifo_count.nom_value;
        if (avg > thr) {
          audio->feedback.value =
              nom + (uint32_t)audio->feedback.compute.fifo_count.rate_const[0] *
                        (avg - thr);
        } else {
          uint32_t drop =
              (uint32_t)audio->feedback.compute.fifo_count.rate_const[1] *
              (thr - avg);
          audio->feedback.value =
              (nom > drop) ? nom - drop : audio->feedback.min_value;
        }
        uint32_t clamped = audio->feedback.value < audio->feedback.min_value
                              ? audio->feedback.min_value
                              : audio->feedback.value;
        audio->feedback.value = clamped > audio->feedback.max_value
                                    ? audio->feedback.max_value
                                    : clamped;
      } break;

      case AUDIO_FEEDBACK_METHOD_FREQUENCY_POWER_OF_2:
        audio->feedback.value = 1UL << audio->feedback.compute.power_of_2;
        break;

      case AUDIO_FEEDBACK_METHOD_FREQUENCY_FLOAT:
        audio->feedback.value =
            (uint32_t)(audio->feedback.compute.float_const *
                       (float)(1UL << (16u - (frame_shift - 1u))));
        break;

      case AUDIO_FEEDBACK_METHOD_FREQUENCY_FIXED: {
        uint32_t frame_div = backend().isFullSpeed() ? 1000u : 8000u;
        audio->feedback.value =
            (audio->feedback.compute.fixed.sample_freq << 16) / frame_div;
      } break;

      default:
        break;
    }

    if (backend().claimEndpoint(audio->rhport, audio->ep_fb)) {
      audiod_fb_send(audio);
    }
  }

  // USBD Driver API — public so a backend's class-driver registration glue
  // (e.g. USBAudioBackendTinyUSB.h's trampoline into TinyUSB's
  // usbd_class_driver_t) can call these without being a member of this
  // class. A native-HAL backend with no such registration concept would
  // instead call these directly from its own ISR/init code.
 public:
  void audiod_init(void) {
    audiod_fct_.resize(getAudioCount());

    // Initialize control buffers
    for (uint8_t i = 0; i < getAudioCount(); i++) {
      audiod_function_t* audio = &audiod_fct_[i];
      // Initialize control buffers
      int size = getCtrlBufSz(i);
      audio->ctrl_buf.resize(size);
      audio->ctrl_buf_sz = size;
      // Initialize active alternate interface buffers
      audio->alt_setting.resize(descr_builder.audioFunctionsCount());
      // Initialize IN EP — lin_buf_in is the DMA staging buffer (one frame).
      // Audio data flows through bufferTx() (resized in begin()), not ep_in_ff.
      if (isEpInEnabled()) {
        // Max packet across all advertised rates (see kSupportedSampleRates).
        uint16_t max_pkt = descr_builder.calcPacketSizeForRate(
            kSupportedSampleRates[kNumSupportedSampleRates - 1]);
        audio->lin_buf_in.resize(max_pkt);
      }
      // Initialize OUT EP linear (DMA staging) buffer.
      if (isEpOutEnabled()) {
        uint16_t max_pkt = descr_builder.calcPacketSizeForRate(
            kSupportedSampleRates[kNumSupportedSampleRates - 1]);
        audio->lin_buf_out.resize(max_pkt);
      }
      if (isFeedbackEpEnabled()) {
        audio->fb_buf.resize(1);  // one uint32_t = 4 bytes of feedback data
      }
    }
  }

  bool audiod_deinit(void) {
    return false;  // TODO not implemented yet
  }

  void audiod_reset(uint8_t rhport) {
    (void)rhport;
    for (uint8_t i = 0; i < getAudioCount(); i++) {
      audiod_function_t* audio = &audiod_fct_[i];
      memset(audio, 0, getResetSize());
      if (isEpInEnabled()) {
        bufferTx().reset();
      }
      if (isEpOutEnabled()) {
        bufferRx().reset();
      }
    }
  }

  uint16_t audiod_open(uint8_t rhport, UsbInterfaceDescriptorView const& itf_desc,
                       uint8_t const* raw_desc, uint16_t max_len) {
    (void)max_len;
    if (!(kUsbClassAudio == itf_desc.bInterfaceClass &&
          kAudioSubclassControl == itf_desc.bInterfaceSubClass))
      return 0;
    if (itf_desc.bInterfaceProtocol != kAudioIntProtocolCodeV2) return 0;
    if (itf_desc.bNumEndpoints > 1) return 0;
    if (itf_desc.bNumEndpoints == 1 && !isInterruptEpEnabled()) return 0;
    if (itf_desc.bAlternateSetting != 0) return 0;
    uint8_t i;
    for (i = 0; i < getAudioCount(); i++) {
      if (!audiod_fct_[i].p_desc) {
        audiod_fct_[i].p_desc = raw_desc;
        audiod_fct_[i].rhport = rhport;
        audiod_fct_[i].desc_length = getDescLen(i);
        // audiod_reset() zeroes ctrl_buf_sz via memset — restore it so
        // controlTransfer() receives the correct buffer length.
        audiod_fct_[i].ctrl_buf_sz = getCtrlBufSz(i);
        if (isEpInEnabled() || isEpOutEnabled() || isFeedbackEpEnabled()) {
          uint8_t ep_in = 0, ep_out = 0, ep_fb = 0;
          uint16_t ep_in_size = 0, ep_out_size = 0;
          UsbEndpointDescriptorView desc_ep_out{};
          bool has_ep_out_view = false;
          uint8_t const* p_desc = audiod_fct_[i].p_desc;
          uint8_t const* p_desc_end =
              p_desc + audiod_fct_[i].desc_length - kAudioDescIadLen;
          while (p_desc_end - p_desc > 0) {
            if (descType(p_desc) == kUsbDescTypeEndpoint) {
              UsbEndpointDescriptorView desc_ep = decodeEndpoint(p_desc);
              if (desc_ep.xferType == UsbXferType::Isochronous) {
                if (isFeedbackEpEnabled() && desc_ep.usage == 1) {
                  ep_fb = desc_ep.bEndpointAddress;
                }
                if (desc_ep.usage == 0) {
                  if (isEpInEnabled() && desc_ep.direction() == UsbDir::In) {
                    ep_in = desc_ep.bEndpointAddress;
                    uint16_t sz = desc_ep.packetSize();
                    ep_in_size = sz > ep_in_size ? sz : ep_in_size;
                  } else if (isEpOutEnabled() &&
                             desc_ep.direction() == UsbDir::Out) {
                    ep_out = desc_ep.bEndpointAddress;
                    uint16_t sz = desc_ep.packetSize();
                    ep_out_size = sz > ep_out_size ? sz : ep_out_size;
                    desc_ep_out = desc_ep;
                    has_ep_out_view = true;
                  }
                }
              }
            }
            p_desc = descNext(p_desc);
          }
          if (isEpInEnabled() && ep_in) {
            bool alloc_ok = backend().isoAllocEndpoint(rhport, ep_in, ep_in_size);
            LOGD("iso_alloc IN ep=0x%02x sz=%u: %s", ep_in, ep_in_size,
                 alloc_ok ? "OK" : "FAIL");
          }
          if (isEpOutEnabled() && ep_out) {
            bool alloc_ok = backend().isoAllocEndpoint(rhport, ep_out, ep_out_size);
            LOGD("iso_alloc OUT ep=0x%02x sz=%u: %s", ep_out, ep_out_size,
                 alloc_ok ? "OK" : "FAIL");
            if (has_ep_out_view) audiod_fct_[i].ep_out_view = desc_ep_out;
            if (backend().usesIsoAlloc() && has_ep_out_view) {
              // Pre-activate during enumeration (no isochronous traffic).
              // Cannot be done in SET_INTERFACE because iso_activate blocks
              // on ESP32's DWC2 when the host is already sending. Activation
              // itself resets the endpoint's busy/claimed state to a known
              // (not busy) baseline, so nothing further is needed here for
              // the first open -- see closeEpOut() for why re-activation is
              // also needed on every subsequent re-open.
              backend().isoActivateEndpoint(rhport, desc_ep_out);
              LOGD("iso_activate OUT: done");
            }
          }
          if (isFeedbackEpEnabled() && ep_fb) {
            backend().isoAllocEndpoint(rhport, ep_fb, 4);
          }
        }
        // Scan for bclock_id_tx (clock entity referenced by the USB-streaming
        // terminal) and interval_tx.  Runs in TX, RX, and RXTX mode so that
        // clock-validity/frequency GET requests always succeed.
        //   TX/RXTX: Output Terminal type=USB_STREAMING → bCSourceID at [8]
        //   RX:      Input  Terminal type=USB_STREAMING → bCSourceID at [7]
        // interval_tx is only meaningful for the ISO IN endpoint (TX/RXTX).
        if (isEpInEnabled() || isEpOutEnabled()) {
          uint8_t const* p_desc = audiod_fct_[i].p_desc;
          uint8_t const* p_desc_end =
              p_desc + audiod_fct_[i].desc_length - kAudioDescIadLen;
          while (p_desc_end - p_desc > 0) {
            if (descType(p_desc) == kUsbDescTypeEndpoint) {
              if (isEpInEnabled()) {
                UsbEndpointDescriptorView desc_ep = decodeEndpoint(p_desc);
                if (desc_ep.xferType == UsbXferType::Isochronous &&
                    desc_ep.usage == 0 && desc_ep.direction() == UsbDir::In) {
                  audiod_fct_[i].interval_tx = desc_ep.bInterval;
                }
              }
            } else if (descType(p_desc) == kUsbDescTypeCsInterface) {
              if (descSubtype(p_desc) == kAudioCsAcOutputTerminal) {
                if (descU16(p_desc + 4) == kAudioTermTypeUsbStreaming) {
                  audiod_fct_[i].bclock_id_tx = p_desc[8];  // OT bCSourceID
                }
              } else if (descSubtype(p_desc) == kAudioCsAcInputTerminal) {
                if (descU16(p_desc + 4) == kAudioTermTypeUsbStreaming) {
                  audiod_fct_[i].bclock_id_tx = p_desc[7];  // IT bCSourceID
                }
              }
            }
            p_desc = descNext(p_desc);
          }
        }

        if (isInterruptEpEnabled()) {
          uint8_t const* p_desc = audiod_fct_[i].p_desc;
          uint8_t const* p_desc_end =
              p_desc + audiod_fct_[i].desc_length - kAudioDescIadLen;
          while (p_desc_end - p_desc > 0) {
            if (descType(p_desc) == kUsbDescTypeEndpoint) {
              UsbEndpointDescriptorView desc_ep = decodeEndpoint(p_desc);
              uint8_t const ep_addr = desc_ep.bEndpointAddress;
              if (desc_ep.direction() == UsbDir::In &&
                  desc_ep.xferType == UsbXferType::Interrupt) {
                if (backend().openEndpoint(audiod_fct_[i].rhport, desc_ep)) {
                  audiod_fct_[i].ep_int = ep_addr;
                } else {
                  LOGE("  UAC2: interrupt EP 0x%02x open failed", ep_addr);

                }
              }
            }
            p_desc = descNext(p_desc);
          }
        }
        audiod_fct_[i].mounted = true;
        break;
      }
    }
    if (i >= getAudioCount()) return 0;
    uint16_t drv_len = audiod_fct_[i].desc_length - kAudioDescIadLen;
    return drv_len;
  }

  bool audiod_control_xfer_cb(uint8_t rhport, uint8_t stage,
                              UsbSetupPacket const& request) {
    if (stage == kControlStageSetup) {
      return audiod_control_request(rhport, request);
    } else if (stage == kControlStageData) {
      return audiod_control_complete(rhport, request);
    }
    return true;
  }
  // Invoked when class request DATA stage is finished.
  // return false to stall control EP (e.g Host send non-sense DATA)
  bool audiod_control_complete(uint8_t rhport,
                               UsbSetupPacket const& p_request) {
    // Handle audio class specific set requests
    if (p_request.type() == UsbSetupPacket::Type::Class &&
        p_request.direction() == UsbDir::Out) {
      uint8_t func_id;

      switch (p_request.recipient()) {
        case UsbSetupPacket::Recipient::Interface: {
          uint8_t itf = u16Low(p_request.wIndex);
          uint8_t entityID = u16High(p_request.wIndex);

          if (entityID != 0) {
            func_id = 0;
            uint8_t ctrlSel = u16High(p_request.wValue);
            uint8_t* cb = audiod_fct_[func_id].ctrl_buf.data();

            // ── Clock Source SET_CUR (sample rate) ──────────────
            if (entityID == USBAudio2DescriptorBuilder::ENTITY_CLOCK &&
                ctrlSel == kAudioCsCtrlSamFreq &&
                p_request.bRequest == kAudioCsReqCur) {
              setSampleRate(descU32(cb));
            }

            // ── Feature Unit SET_CUR (mute / volume) ────────────
            if (isFeatureUnit(entityID) &&
                p_request.bRequest == kAudioCsReqCur) {
              uint8_t channel = u16Low(p_request.wValue);
              if (ctrlSel == AUDIO_FU_CTRL_MUTE) {
                setMute(cb[0] != 0, channel);
              } else if (ctrlSel == AUDIO_FU_CTRL_VOLUME) {
                int16_t v;
                memcpy(&v, cb, 2);
                setVolume(uac2ToFloat(v), channel);
              }
            }

            // Invoke callback
            if (tud_audio_set_req_entity_cb_) {
              return tud_audio_set_req_entity_cb_(this, rhport, p_request, cb);
            }
          } else {
            // Find index of audio driver structure and verify interface really
            // exists
            if (!audiod_verify_itf_exists(itf, &func_id)) return false;

            // Invoke callback
            if (tud_audio_set_req_itf_cb_) {
              return tud_audio_set_req_itf_cb_(
                  this, rhport, p_request,
                  audiod_fct_[func_id].ctrl_buf.data());
            }
          }
        } break;

        case UsbSetupPacket::Recipient::Endpoint: {
          uint8_t ep = u16Low(p_request.wIndex);

          // Check if entity is present and get corresponding driver index
          if (!audiod_verify_ep_exists(ep, &func_id)) return false;

          // Invoke callback
          if (tud_audio_set_req_ep_cb_) {
            return tud_audio_set_req_ep_cb_(
                this, rhport, p_request, audiod_fct_[func_id].ctrl_buf.data());
          }
        } break;
        // Unknown/Unsupported recipient
        default:
          return false;
      }
    }
    return true;
  }

  /// TODO refactor control request handling to separate function and reduce nesting
  bool audiod_xfer_cb(uint8_t rhport, uint8_t ep_addr, UsbXferResult result,
                      uint32_t xferred_bytes) {
    (void)result;
    (void)xferred_bytes;
    for (uint8_t func_id = 0; func_id < getAudioCount(); func_id++) {
      audiod_function_t* audio = &audiod_fct_[func_id];
      if (isInterruptEpEnabled() && audio->ep_int == ep_addr) {
        if (int_done_cb_) int_done_cb_(this, rhport);
        return true;
      }
      if (isEpInEnabled() && audio->ep_in == ep_addr &&
          audio->alt_setting.size() != 0) {
        xfer_cb_tx_count_ = xfer_cb_tx_count_ + 1;
        if (tx_done_cb_) tx_done_cb_(this, rhport, audio);

        uint16_t frame_bytes = isEpInFlowControlEnabled()
                                   ? audiod_tx_packet_size_fc(audio)
                                   : audio->ep_in_sz;
        if (frame_bytes > audio->ep_in_sz) frame_bytes = audio->ep_in_sz;
        tx_frame_bytes_last_ = frame_bytes;
        tx_xferred_last_ = xferred_bytes;

        // Drain platform buffer into lin_buf_in, zero-pad, send via DMA.
        {
          uint8_t* dst = audio->lin_buf_in.data();
          uint16_t n = (uint16_t)bufferTx().readArray(dst, frame_bytes);
          tx_fifo_read_total_ += n;
          if (n < frame_bytes) memset(dst + n, 0, frame_bytes - n);
          (void)backend().transfer(rhport, audio->ep_in, dst, frame_bytes);
        }
        return true;
      }
      if (isEpOutEnabled() && audio->ep_out == ep_addr) {
        xfer_cb_rx_count_ = xfer_cb_rx_count_ + 1;
        rx_total_bytes_ += xferred_bytes;
        // Copy DMA-received data into the platform buffer, re-arm DMA.
        if (xferred_bytes > 0)
          bufferRx().writeArray(audio->lin_buf_out.data(),
                                (int)xferred_bytes);
        if (rx_done_cb_)
          rx_done_cb_(this, rhport, audio, (uint16_t)xferred_bytes);
        (void)backend().transfer(rhport, audio->ep_out,
                                 audio->lin_buf_out.data(), audio->ep_out_sz);
        return true;
      }
      if (isFeedbackEpEnabled() && audio->ep_fb == ep_addr) {
        // SOF ISR owns re-sending; just notify the application.
        if (fb_done_cb_) fb_done_cb_(this, func_id);
        return true;
      }
    }
    return false;
  }

  void audiod_sof_isr(uint8_t rhport, uint32_t frame_count) {
    (void)rhport;
    (void)frame_count;
    if (isEpOutEnabled() && isFeedbackEpEnabled()) {
      for (uint8_t i = 0; i < getAudioCount(); i++) {
        audiod_function_t* audio = &audiod_fct_[i];
        if (audio->ep_fb != 0) {
          uint8_t const hs_adjust = backend().isHighSpeed() ? 3 : 0;
          uint32_t const interval =
              1UL << (audio->feedback.frame_shift - hs_adjust);
          if (0 == (frame_count & (interval - 1))) {
            tud_audio_feedback_interval_isr(i, frame_count,
                                            audio->feedback.frame_shift);
          }
        }
      }
    }
  }

 protected:
  // ── Clock Source GET handler ────────────────────────────────────────────
  bool handleClockSourceGet(uint8_t rhport,
                            UsbSetupPacket const& p_request,
                            uint8_t* cb) {
    uint8_t ctrlSel = u16High(p_request.wValue);
    if (ctrlSel == kAudioCsCtrlClkValid &&
        p_request.bRequest == kAudioCsReqCur) {
      cb[0] = 1;
      return backend().controlTransfer(rhport, p_request, cb, 1);
    }
    if (ctrlSel == kAudioCsCtrlSamFreq) {
      uint32_t rate = (uint32_t)config_.sample_rate;
      if (p_request.bRequest == kAudioCsReqCur) {
        memcpy(cb, &rate, 4);
        return backend().controlTransfer(rhport, p_request, cb, 4);
      }
      if (p_request.bRequest == kAudioCsReqRange) {
        if (config_.enable_multi_sample_rate) {
          // List all supported discrete rates
          uint16_t cnt = kNumSupportedSampleRates;
          memcpy(cb, &cnt, 2);
          for (uint8_t i = 0; i < kNumSupportedSampleRates; i++) {
            uint32_t r = kSupportedSampleRates[i];
            uint32_t z = 0;
            memcpy(cb + 2 + i * 12,     &r, 4);
            memcpy(cb + 2 + i * 12 + 4, &r, 4);
            memcpy(cb + 2 + i * 12 + 8, &z, 4);
          }
          return backend().controlTransfer(
              rhport, p_request, cb,
              (uint16_t)(2 + kNumSupportedSampleRates * 12));
        } else {
          // Single fixed rate from config
          uint16_t cnt = 1;
          uint32_t z = 0;
          memcpy(cb,      &cnt,  2);
          memcpy(cb + 2,  &rate, 4);  // dMIN
          memcpy(cb + 6,  &rate, 4);  // dMAX
          memcpy(cb + 10, &z,    4);  // dRES = 0 (fixed)
          return backend().controlTransfer(rhport, p_request, cb, 14);
        }
      }
    }
    return false;
  }

  // ── Feature Unit GET handler ──────────────────────────────────────────
  bool handleFeatureUnitGet(uint8_t rhport,
                            UsbSetupPacket const& p_request,
                            uint8_t* cb) {
    uint8_t ctrlSel = u16High(p_request.wValue);
    uint8_t channel = u16Low(p_request.wValue);
    if (ctrlSel == AUDIO_FU_CTRL_MUTE &&
        p_request.bRequest == kAudioCsReqCur) {
      cb[0] = isMute(channel) ? 1 : 0;
      return backend().controlTransfer(rhport, p_request, cb, 1);
    }
    if (ctrlSel == AUDIO_FU_CTRL_VOLUME) {
      if (p_request.bRequest == kAudioCsReqCur) {
        int16_t v = floatToUac2(volume(channel));
        memcpy(cb, &v, 2);
        return backend().controlTransfer(rhport, p_request, cb, 2);
      }
      if (p_request.bRequest == kAudioCsReqRange) {
        uint16_t cnt = 1;
        int16_t vmin = -25600, vmax = 0, vres = 256;
        memcpy(cb + 0, &cnt, 2);
        memcpy(cb + 2, &vmin, 2);
        memcpy(cb + 4, &vmax, 2);
        memcpy(cb + 6, &vres, 2);
        return backend().controlTransfer(rhport, p_request, cb, 8);
      }
    }
    return false;
  }

  // ── Entity request handler (Clock Source + Feature Unit) ──────────────
  bool handleEntityRequest(uint8_t rhport,
                           UsbSetupPacket const& p_request,
                           uint8_t entityID) {
    uint8_t func_id = 0;
    uint8_t* cb = audiod_fct_[func_id].ctrl_buf.data();
    bool is_get = (p_request.direction() == UsbDir::In);

    if (entityID == USBAudio2DescriptorBuilder::ENTITY_CLOCK) {
      if (is_get && handleClockSourceGet(rhport, p_request, cb)) return true;
      // SET — schedule data receive for audiod_control_complete()
      return backend().controlTransfer(rhport, p_request, cb,
                                       audiod_fct_[func_id].ctrl_buf_sz);
    }

    if (isFeatureUnit(entityID)) {
      if (is_get && handleFeatureUnitGet(rhport, p_request, cb)) return true;
      // SET — schedule data receive
      return backend().controlTransfer(rhport, p_request, cb,
                                       audiod_fct_[func_id].ctrl_buf_sz);
    }

    // Unknown entity — try generic verify
    uint8_t itf = u16Low(p_request.wIndex);
    if (!audiod_verify_entity_exists(itf, entityID, &func_id)) {
      backend().controlStatus(rhport, p_request);
      return true;
    }
    if (is_get && req_entity_cb_) return req_entity_cb_(this, func_id);
    return backend().controlTransfer(rhport, p_request,
                                     audiod_fct_[func_id].ctrl_buf.data(),
                                     audiod_fct_[func_id].ctrl_buf_sz);
  }

  // ── Interface request handler (entityID == 0) ─────────────────────────
  bool handleInterfaceRequest(uint8_t rhport,
                              UsbSetupPacket const& p_request) {
    uint8_t itf = u16Low(p_request.wIndex);
    uint8_t func_id;
    if (!audiod_verify_itf_exists(itf, &func_id)) return false;
    if (p_request.direction() == UsbDir::In) {
      if (get_req_itf_cb_) return get_req_itf_cb_(this, rhport, p_request);
      return false;
    }
    return backend().controlTransfer(rhport, p_request,
                                     audiod_fct_[func_id].ctrl_buf.data(),
                                     audiod_fct_[func_id].ctrl_buf_sz);
  }

  // ── Endpoint request handler ──────────────────────────────────────────
  bool handleEndpointRequest(uint8_t rhport,
                             UsbSetupPacket const& p_request) {
    uint8_t ep = u16Low(p_request.wIndex);
    uint8_t func_id;
    if (!audiod_verify_ep_exists(ep, &func_id)) return false;
    if (p_request.direction() == UsbDir::In) {
      if (get_req_ep_cb_) return get_req_ep_cb_(this, rhport, p_request);
      return false;
    }
    return backend().controlTransfer(rhport, p_request,
                                     audiod_fct_[func_id].ctrl_buf.data(),
                                     audiod_fct_[func_id].ctrl_buf_sz);
  }

  // ── Main control request dispatcher ───────────────────────────────────
  bool audiod_control_request(uint8_t rhport,
                              UsbSetupPacket const& p_request) {
    if (p_request.type() == UsbSetupPacket::Type::Standard) {
      switch (p_request.bRequest) {
        case (uint8_t)UsbStdRequest::GetInterface:
          return audiod_get_interface(rhport, p_request);
        case (uint8_t)UsbStdRequest::SetInterface:
          return audiod_set_interface(rhport, p_request);
        case (uint8_t)UsbStdRequest::ClearFeature:
          return true;
        default:
          return false;
      }
    }

    if (p_request.type() == UsbSetupPacket::Type::Class) {
      switch (p_request.recipient()) {
        case UsbSetupPacket::Recipient::Interface: {
          uint8_t entityID = u16High(p_request.wIndex);
          return (entityID != 0)
                     ? handleEntityRequest(rhport, p_request, entityID)
                     : handleInterfaceRequest(rhport, p_request);
        }
        case UsbSetupPacket::Recipient::Endpoint:
          return handleEndpointRequest(rhport, p_request);
        default:
          return false;
      }
    }

    return false;
  }

  // Verify an entity with the given ID exists and returns also the
  // corresponding driver index
  bool audiod_verify_entity_exists(uint8_t itf, uint8_t entityID,
                                   uint8_t* func_id) {
    uint8_t i;
    for (i = 0; i < getAudioCount(); i++) {
      // Look for the correct driver by checking if the unique standard AC
      // interface number fits
      if (audiod_fct_[i].p_desc &&
          decodeInterface(audiod_fct_[i].p_desc).bInterfaceNumber == itf) {
        // Get pointers after class specific AC descriptors and end of AC
        // descriptors - entities are defined in between
        uint8_t const* p_desc =
            descNext(audiod_fct_[i].p_desc);  // Points to CS AC descriptor
        // AC interface header wTotalLength lives at byte offset 6 (UAC2
        // Table 4-5: bLength,bDescriptorType,bDescriptorSubType,bcdADC(2),
        // bCategory,wTotalLength(2),bmControls).
        uint8_t const* p_desc_end = descU16(p_desc + 6) + p_desc;
        p_desc = descNext(p_desc);  // Get past CS AC descriptor

        // Condition modified from p_desc < p_desc_end to prevent gcc>=12
        // strict-overflow warning
        while (p_desc_end - p_desc > 0) {
          if (p_desc[3] == entityID)  // Entity IDs are always at offset 3
          {
            *func_id = i;
            return true;
          }
          p_desc = descNext(p_desc);
        }
      }
    }
    return false;
  }

  bool audiod_verify_ep_exists(uint8_t ep, uint8_t* func_id) {
    uint8_t i;
    for (i = 0; i < getAudioCount(); i++) {
      if (audiod_fct_[i].p_desc) {
        // Get pointer at end
        uint8_t const* p_desc_end =
            audiod_fct_[i].p_desc + audiod_fct_[i].desc_length;

        // Advance past AC descriptors - EP we look for are streaming EPs
        uint8_t const* p_desc = descNext(audiod_fct_[i].p_desc);
        p_desc += descU16(p_desc + 6);  // AC header wTotalLength, see above

        // Condition modified from p_desc < p_desc_end to prevent gcc>=12
        // strict-overflow warning
        while (p_desc_end - p_desc > 0) {
          if (descType(p_desc) == kUsbDescTypeEndpoint &&
              decodeEndpoint(p_desc).bEndpointAddress == ep) {
            *func_id = i;
            return true;
          }
          p_desc = descNext(p_desc);
        }
      }
    }
    return false;
  }

  bool audiod_verify_itf_exists(uint8_t itf, uint8_t* func_id) {
    uint8_t i;
    for (i = 0; i < getAudioCount(); i++) {
      if (audiod_fct_[i].p_desc) {
        // Get pointer at beginning and end
        uint8_t const* p_desc = audiod_fct_[i].p_desc;
        uint8_t const* p_desc_end = audiod_fct_[i].p_desc +
                                    audiod_fct_[i].desc_length -
                                    kAudioDescIadLen;
        // Condition modified from p_desc < p_desc_end to prevent gcc>=12
        // strict-overflow warning
        while (p_desc_end - p_desc > 0) {
          if (descType(p_desc) == kUsbDescTypeInterface &&
              decodeInterface(audiod_fct_[i].p_desc).bInterfaceNumber == itf) {
            *func_id = i;
            return true;
          }
          p_desc = descNext(p_desc);
        }
      }
    }
    return false;
  }

  void audiod_parse_flow_control_params(audiod_function_t* audio,
                                        uint8_t const* p_desc) {
    // Seed the TX sample rate from the configured AudioInfo so packet-size
    // calculation works even when the host never issues a SET_CUR(SAM_FREQ)
    // request (typical for single-frequency clock sources). The host may still
    // override this later via audiod_control_complete().
    if (audio->sample_rate_tx == 0)
      audio->sample_rate_tx = (uint32_t)config_.sample_rate;

    p_desc = descNext(p_desc);  // Exclude standard AS interface descriptor
                                // of current alternate interface descriptor

    // Look for a Class-Specific AS Interface Descriptor(4.9.2) to verify format
    // type and format and also to get number of physical channels.
    // Layout (UAC2 Table 4-27): bLength,bDescriptorType,bDescriptorSubType,
    // bTerminalLink,bmControls,bFormatType[5],bmFormats(4),bNrChannels[10],...
    if (descType(p_desc) == kUsbDescTypeCsInterface &&
        descSubtype(p_desc) == kAudioCsAsGeneral) {
      audio->n_channels_tx = p_desc[10];
      audio->format_type_tx = (audio_format_type_t)p_desc[5];
      // Look for a Type I Format Type Descriptor(2.3.1.6 - Audio Formats)
      // Layout: bLength,bDescriptorType,bDescriptorSubType,bFormatType,
      // bSubslotSize[4],bBitResolution.
      p_desc = descNext(p_desc);
      if (descType(p_desc) == kUsbDescTypeCsInterface &&
          descSubtype(p_desc) == kAudioCsAsFormatType &&
          p_desc[3] == AUDIO_FORMAT_TYPE_I) {
        audio->n_bytes_per_sample_tx = p_desc[4];
      }
    }

    // Fallback from config if descriptor parsing missed any field.
    // The descriptor struct layout may differ across TinyUSB versions.
    if (audio->n_channels_tx == 0) audio->n_channels_tx = config_.channels;
    if (audio->n_bytes_per_sample_tx == 0)
      audio->n_bytes_per_sample_tx = config_.bits_per_sample / 8;
    if (audio->format_type_tx == 0) audio->format_type_tx = AUDIO_FORMAT_TYPE_I;
  }

  // This helper function finds for a given audio function and AS interface
  // number the index of the attached driver structure, the index of the
  // interface in the audio function
  // (e.g. the std. AS interface with interface number 15 is the first AS
  // interface for the given audio function and thus gets index zero), and
  // finally a pointer to the std. AS interface, where the pointer always points
  // to the first alternate setting i.e. alternate interface zero.
  bool audiod_get_AS_interface_index(uint8_t itf, audiod_function_t* audio,
                                     uint8_t* idxItf,
                                     uint8_t const** pp_desc_int) {
    if (audio->p_desc) {
      // Get pointer at end
      uint8_t const* p_desc_end =
          audio->p_desc + audio->desc_length - kAudioDescIadLen;

      // Advance past AC descriptors
      uint8_t const* p_desc = descNext(audio->p_desc);
      p_desc += descU16(p_desc + 6);  // AC header wTotalLength

      uint8_t tmp = 0;
      // Condition modified from p_desc < p_desc_end to prevent gcc>=12
      // strict-overflow warning
      while (p_desc_end - p_desc > 0) {
        // We assume the number of alternate settings is increasing thus we
        // return the index of alternate setting zero!
        if (descType(p_desc) == kUsbDescTypeInterface &&
            decodeInterface(p_desc).bAlternateSetting == 0) {
          if (decodeInterface(p_desc).bInterfaceNumber == itf) {
            *idxItf = tmp;
            *pp_desc_int = p_desc;
            return true;
          }
          // Increase index, bytes read, and pointer
          tmp++;
        }
        p_desc = descNext(p_desc);
      }
    }
    return false;
  }

  // This helper function finds for a given AS interface number the index of the
  // attached driver structure, the index of the interface in the audio function
  // (e.g. the std. AS interface with interface number 15 is the first AS
  // interface for the given audio function and thus gets index zero), and
  // finally a pointer to the std. AS interface, where the pointer always points
  // to the first alternate setting i.e. alternate interface zero.
  bool audiod_get_AS_interface_index_global(uint8_t itf, uint8_t* func_id,
                                            uint8_t* idxItf,
                                            uint8_t const** pp_desc_int) {
    // Loop over audio driver interfaces
    uint8_t i;
    for (i = 0; i < getAudioCount(); i++) {
      if (audiod_get_AS_interface_index(itf, &audiod_fct_[i], idxItf,
                                        pp_desc_int)) {
        *func_id = i;
        return true;
      }
    }

    return false;
  }

  bool audiod_get_interface(uint8_t rhport,
                            UsbSetupPacket const& p_request) {
    uint8_t const itf = u16Low(p_request.wIndex);

    // Find index of audio streaming interface
    uint8_t func_id, idxItf;
    uint8_t const* dummy;

    if (!audiod_get_AS_interface_index_global(itf, &func_id, &idxItf, &dummy))
      return false;
    if (!backend().controlTransfer(
            rhport, p_request, &audiod_fct_[func_id].alt_setting[idxItf], 1))
      return false;

    LOGI("  Get itf: %u - current alt: %u", itf,
            audiod_fct_[func_id].alt_setting[idxItf]);

    return true;
  }

  bool audiod_fb_send(audiod_function_t* audio) {
    bool apply_correction =
        backend().isFullSpeed() && audio->feedback.format_correction;
    // Format the feedback value
    if (apply_correction) {
      uint8_t* fb = (uint8_t*)audio->fb_buf.data();

      // For FS format is 10.14
      *(fb++) = (audio->feedback.value >> 2) & 0xFF;
      *(fb++) = (audio->feedback.value >> 10) & 0xFF;
      *(fb++) = (audio->feedback.value >> 18) & 0xFF;
      *fb = 0;
    } else {
      audio->fb_buf[0] = audio->feedback.value;
    }

    // About feedback format on FS
    //
    // 3 variables: Format | packetSize | sendSize | Working OS:
    //              16.16    4            4          Linux, Windows
    //              16.16    4            3          Linux
    //              16.16    3            4          Linux
    //              16.16    3            3          Linux
    //              10.14    4            4          Linux
    //              10.14    4            3          Linux
    //              10.14    3            4          Linux, OSX
    //              10.14    3            3          Linux, OSX
    //
    // We send 3 bytes since sending packet larger than wMaxPacketSize is pretty
    // ugly
    return backend().transfer(audio->rhport, audio->ep_fb,
                              (uint8_t*)audio->fb_buf.data(),
                              apply_correction ? 3 : 4);
  }

  // ── Close existing EPs for this interface ──────────────────────────────
  void closeEpIn(uint8_t rhport, audiod_function_t* audio, uint8_t itf,
                 UsbSetupPacket const& p_request) {
    if (!isEpInEnabled() || audio->ep_in_as_intf_num != itf) return;
    audio->ep_in_as_intf_num = 0;
    if (!backend().usesIsoAlloc()) backend().closeEndpoint(rhport, audio->ep_in);
    bufferTx().reset();
    if (tud_audio_set_itf_close_EP_cb_)
      tud_audio_set_itf_close_EP_cb_(this, rhport, p_request);
    audio->ep_in = 0;
    if (isEpInFlowControlEnabled()) {
      audio->packet_sz_tx[0] = 0;
      audio->packet_sz_tx[1] = 0;
      audio->packet_sz_tx[2] = 0;
    }
    notifyStreamingState();
  }

  void closeEpOut(uint8_t rhport, audiod_function_t* audio, uint8_t itf,
                  UsbSetupPacket const& p_request) {
    if (!isEpOutEnabled() || audio->ep_out_as_intf_num != itf) return;
    audio->ep_out_as_intf_num = 0;
    if (!backend().usesIsoAlloc()) {
      backend().closeEndpoint(rhport, audio->ep_out);
    } else {
      // usesIsoAlloc() backends never close the OUT endpoint between
      // sessions (DPRAM stays allocated), so nothing resets the DCD's
      // internal busy/claimed bookkeeping for it. If the transfer armed by
      // the *previous* openEpOut() never completed (no host data arrived,
      // or the session was torn down mid-flight), that endpoint stays
      // "busy" forever and every later usbd_edpt_xfer() call in openEpOut()
      // silently fails, permanently breaking RX after the first
      // open/close cycle. Re-activating here -- while the host is quiet
      // (it just asked to close the stream) rather than right as a new
      // session starts -- resets busy/claimed to a known-good state for
      // the next open, matching the reasoning in audiod_open() for why
      // this can't be done at SET_INTERFACE(alt=1) time instead.
      backend().isoActivateEndpoint(rhport, audio->ep_out_view);
    }
    if (tud_audio_set_itf_close_EP_cb_)
      tud_audio_set_itf_close_EP_cb_(this, rhport, p_request);
    audio->ep_out = 0;
    if (isFeedbackEpEnabled()) {
      audio->ep_fb = 0;
      memset(&audio->feedback, 0, sizeof(audio->feedback));
    }
    notifyStreamingState();
  }

  // ── Activate a single endpoint found in the descriptor ────────────────
  bool activateEndpoint(uint8_t rhport, const UsbEndpointDescriptorView& desc_ep,
                        UsbDir dir = UsbDir::In) {
    if (backend().usesIsoAlloc()) {
      // Skip iso_activate for isochronous OUT — on ESP32's DWC2 it blocks
      // for the entire playback duration.  The endpoint DPRAM was already
      // allocated by iso_alloc in audiod_open().  The XFER call in
      // openEpOut will configure the DCD to receive.
      if (dir == UsbDir::Out && desc_ep.xferType == UsbXferType::Isochronous)
        return true;
      return backend().isoActivateEndpoint(rhport, desc_ep);
    }
    (void)dir;
    return backend().openEndpoint(rhport, desc_ep);
  }

  // ── Open the IN (TX) data endpoint ────────────────────────────────────
  void openEpIn(uint8_t rhport, audiod_function_t* audio, uint8_t itf,
                const UsbEndpointDescriptorView& desc_ep,
                uint8_t const* p_desc_for_params) {
    audio->ep_in = desc_ep.bEndpointAddress;
    audio->ep_in_as_intf_num = itf;
    audio->ep_in_sz = desc_ep.packetSize();
    if (audio->ep_in_sz == 0) return;

    if (isEpInFlowControlEnabled())
      audiod_parse_flow_control_params(audio, p_desc_for_params);

    // Arm initial transfer (silence — copier fills the buffer).
    uint16_t first_pkt = packetSize();
    if (first_pkt > audio->ep_in_sz) first_pkt = audio->ep_in_sz;
    audio->lin_buf_in.assign(audio->ep_in_sz, 0);
    tx_xfer_armed_ = backend().transfer(rhport, audio->ep_in,
                                        audio->lin_buf_in.data(), first_pkt);
    notifyStreamingState();
  }

  // ── Open the OUT (RX) data endpoint ───────────────────────────────────
  void openEpOut(uint8_t rhport, audiod_function_t* audio, uint8_t itf,
                 const UsbEndpointDescriptorView& desc_ep) {
    audio->ep_out = desc_ep.bEndpointAddress;
    audio->ep_out_as_intf_num = itf;
    audio->ep_out_sz = desc_ep.packetSize();
    if (audio->ep_out_sz == 0) return;

    // iso_activate was done in audiod_open() (no traffic, instant).
    // Just arm the transfer here.
    if (audio->lin_buf_out.size() < audio->ep_out_sz)
      audio->lin_buf_out.assign(audio->ep_out_sz, 0);
    backend().transfer(rhport, audio->ep_out, audio->lin_buf_out.data(),
                       audio->ep_out_sz);
    notifyStreamingState();
  }

  // ── Open the explicit feedback endpoint ───────────────────────────────
  void openEpFeedback(audiod_function_t* audio,
                      const UsbEndpointDescriptorView& desc_ep) {
    audio->ep_fb = desc_ep.bEndpointAddress;
    audio->feedback.frame_shift = desc_ep.bInterval - 1;
  }

  // ── Configure feedback computation parameters ─────────────────────────
  void setupFeedback(audiod_function_t* audio, uint8_t func_id, uint8_t alt) {
    if (!isFeedbackEpEnabled() || audio->ep_fb == 0) return;

    audio_feedback_params_t fb_param = {};
    fb_param.method = AUDIO_FEEDBACK_METHOD_FIFO_COUNT;
    fb_param.sample_freq = config_.sample_rate;
    if (tud_audio_feedback_params_cb_)
      tud_audio_feedback_params_cb_(this, func_id, alt, &fb_param);
    audio->feedback.compute_method = fb_param.method;

    if (backend().isFullSpeed() && tud_audio_feedback_format_correction_cb_)
      audio->feedback.format_correction =
          tud_audio_feedback_format_correction_cb_(this, func_id);

    uint32_t const frame_div = backend().isFullSpeed() ? 1000 : 8000;
    audio->feedback.min_value = ((fb_param.sample_freq - 1) / frame_div) << 16;
    audio->feedback.max_value = (fb_param.sample_freq / frame_div + 1) << 16;

    switch (fb_param.method) {
      case AUDIO_FEEDBACK_METHOD_FREQUENCY_FIXED:
      case AUDIO_FEEDBACK_METHOD_FREQUENCY_FLOAT:
      case AUDIO_FEEDBACK_METHOD_FREQUENCY_POWER_OF_2:
        audiod_set_fb_params_freq(audio, fb_param.sample_freq,
                                  fb_param.frequency.mclk_freq);
        break;
      case AUDIO_FEEDBACK_METHOD_FIFO_COUNT: {
        // Use bufferRx() size — ep_out_ff may be uninitialized in linear buffer mode
        uint16_t fifo_depth = bufferRx().size();
        if (fifo_depth == 0) fifo_depth = 1;  // guard against div-by-zero
        uint16_t fifo_lvl_thr = fifo_depth / 2;
        audio->feedback.compute.fifo_count.fifo_lvl_thr = fifo_lvl_thr;
        // fifo_lvl_avg is a Q8 exponential moving average (see the update
        // formula in tud_audio_feedback_interval_isr: `avg = fifo_lvl_avg
        // >> 8`, fed by `ff_count << 8` each tick) -- seed it in the same
        // Q8 scale. It was previously seeded as `fifo_lvl_thr << 16` (Q16,
        // 256x too large), which pinned the very first ~1400 feedback
        // updates (~1.4s at 1kHz) at feedback.max_value before decaying
        // into a sane range.
        audio->feedback.compute.fifo_count.fifo_lvl_avg =
            ((uint32_t)fifo_lvl_thr) << 8;
        uint32_t nominal =
            ((fb_param.sample_freq / 100) << 16) / (frame_div / 100);
        audio->feedback.compute.fifo_count.nom_value = nominal;
        audio->feedback.compute.fifo_count.rate_const[0] =
            (uint16_t)((audio->feedback.max_value - nominal) / fifo_lvl_thr);
        audio->feedback.compute.fifo_count.rate_const[1] =
            (uint16_t)((nominal - audio->feedback.min_value) / fifo_lvl_thr);
        if (backend().isHighSpeed()) {
          audio->feedback.compute.fifo_count.rate_const[0] /= 8;
          audio->feedback.compute.fifo_count.rate_const[1] /= 8;
        }
      } break;
      default:
        break;
    }
  }

  // ── Scan descriptor for endpoints and open them ───────────────────────
  bool openEndpointsForAltSetting(uint8_t rhport, audiod_function_t* audio,
                                  uint8_t func_id, uint8_t itf, uint8_t alt) {
    uint8_t const* p_desc = audio->p_desc;
    uint8_t const* p_desc_end =
        p_desc + audio->desc_length - kAudioDescIadLen;
    LOGD("  openEPs: p_desc=%p end=%p len=%u itf=%u alt=%u",
         p_desc, p_desc_end, audio->desc_length, itf, alt);

    while (p_desc_end - p_desc > 0) {
      if (descType(p_desc) == kUsbDescTypeInterface &&
          decodeInterface(p_desc).bInterfaceNumber == itf &&
          decodeInterface(p_desc).bAlternateSetting == alt) {
        uint8_t const* p_desc_for_params =
            (isEpInEnabled() && isEpInFlowControlEnabled()) ? p_desc : nullptr;
        uint8_t foundEPs = 0;
        uint8_t nEps = decodeInterface(p_desc).bNumEndpoints;
        LOGD("  matched itf=%u alt=%u nEps=%u", itf, alt, nEps);

        while (foundEPs < nEps && (p_desc_end - p_desc > 0)) {
          LOGD("  scan: type=0x%02x len=%u offset=%d",
               p_desc[1], p_desc[0], (int)(p_desc - audio->p_desc));
          if (descType(p_desc) == kUsbDescTypeEndpoint) {
            UsbEndpointDescriptorView desc_ep = decodeEndpoint(p_desc);

            LOGD("  activating ep=0x%02x type=%u...",
                 desc_ep.bEndpointAddress, (unsigned)desc_ep.xferType);
            if (!activateEndpoint(rhport, desc_ep, desc_ep.direction())) {
              LOGD("  activateEndpoint FAILED");
              p_desc = descNext(p_desc);
              continue;
            }
            LOGD("  activated OK");
            // Skip clear_stall for isochronous OUT (iso_activate was also
            // skipped).  For other endpoints, clear the stall as usual.
            if (!(desc_ep.direction() == UsbDir::Out &&
                  desc_ep.xferType == UsbXferType::Isochronous))
              backend().clearStall(rhport, desc_ep.bEndpointAddress);

            uint8_t ep_addr = desc_ep.bEndpointAddress;
            if (isEpInEnabled() && desc_ep.direction() == UsbDir::In &&
                desc_ep.usage == 0x00)
              openEpIn(rhport, audio, itf, desc_ep, p_desc_for_params);

            if (isEpOutEnabled()) {
              if (desc_ep.direction() == UsbDir::Out)
                openEpOut(rhport, audio, itf, desc_ep);
              if (isFeedbackEpEnabled() &&
                  desc_ep.direction() == UsbDir::In && desc_ep.usage == 1)
                openEpFeedback(audio, desc_ep);
            }
            foundEPs += 1;
          }
          p_desc = descNext(p_desc);
        }

        if (foundEPs != nEps) return true;  // ZLP already sent

        if (tud_audio_set_itf_cb_)
          tud_audio_set_itf_cb_(this, rhport, UsbSetupPacket{});

        setupFeedback(audio, func_id, alt);
        return true;
      }
      p_desc = descNext(p_desc);
    }
    return true;
  }

  // ── Main SET_INTERFACE handler ────────────────────────────────────────
  bool audiod_set_interface(uint8_t rhport,
                            UsbSetupPacket const& p_request) {
    uint8_t const itf = u16Low(p_request.wIndex);
    uint8_t const alt = u16Low(p_request.wValue);
    LOGD("SET_ITF itf=%u alt=%u [start]", itf, alt);

    uint8_t func_id, idxItf;
    uint8_t const* p_desc;
    if (!audiod_get_AS_interface_index_global(itf, &func_id, &idxItf,
                                              &p_desc)) {
      LOGD("  AS interface %u not found", itf);
      backend().controlStatus(rhport, p_request);
      return true;
    }
    LOGD("  found func=%u idx=%u", func_id, idxItf);

    audiod_function_t* audio = &audiod_fct_[func_id];

    // 1. Close existing EPs
    LOGD("  close EPs");
    closeEpIn(rhport, audio, itf, p_request);
    closeEpOut(rhport, audio, itf, p_request);

    // 2. Save alt setting and acknowledge
    audio->alt_setting[idxItf] = alt;

    backend().controlStatus(rhport, p_request);
    openEndpointsForAltSetting(rhport, audio, func_id, itf, alt);

    // 4. Update SOF and flow control
    if (isFeedbackEpEnabled()) {
      bool enable_sof = false;
      for (uint8_t i = 0; i < getAudioCount(); i++) {
        if (audiod_fct_[i].ep_fb != 0) {
          enable_sof = true;
          break;
        }
      }
      backend().enableSof(rhport, enable_sof);
    }
    if (isEpInEnabled() && isEpInFlowControlEnabled())
      audiod_calc_tx_packet_sz(audio);

    return true;
  }

  static bool isPowerOfTwo(uint32_t value) {
    return value != 0 && (value & (value - 1)) == 0;
  }

  static uint8_t log2Floor(uint32_t value) {
    uint8_t result = 0;
    while ((value >>= 1u) != 0u) result++;
    return result;
  }

  bool audiod_set_fb_params_freq(audiod_function_t* audio, uint32_t sample_freq,
                                 uint32_t mclk_freq) {
    // Check if frame interval is within sane limits
    // The interval value n_frames was taken from the descriptors within

    // n_frames_min is ceil(2^10 * f_s / f_m) for full speed and ceil(2^13 * f_s
    // / f_m) for high speed this lower limit ensures the measures feedback
    // value has sufficient precision
    uint32_t const k = backend().isFullSpeed() ? 10 : 13;
    uint32_t const n_frame = (1UL << audio->feedback.frame_shift);

    if ((((1UL << k) * sample_freq / mclk_freq) + 1) > n_frame) {
      LOGE("  UAC2 feedback interval too small");
      return false;
    }

    // Check if parameters really allow for a power of two division
    if ((mclk_freq % sample_freq) == 0 &&
        isPowerOfTwo(mclk_freq / sample_freq)) {
      audio->feedback.compute_method =
          AUDIO_FEEDBACK_METHOD_FREQUENCY_POWER_OF_2;
      audio->feedback.compute.power_of_2 =
          (uint8_t)(16 - (audio->feedback.frame_shift - 1) -
                    log2Floor(mclk_freq / sample_freq));
    } else if (audio->feedback.compute_method ==
               AUDIO_FEEDBACK_METHOD_FREQUENCY_FLOAT) {
      audio->feedback.compute.float_const =
          (float)sample_freq / (float)mclk_freq *
          (1UL << (16 - (audio->feedback.frame_shift - 1)));
    } else {
      audio->feedback.compute.fixed.sample_freq = sample_freq;
      audio->feedback.compute.fixed.mclk_freq = mclk_freq;
    }

    return true;
  }

  bool audiod_calc_tx_packet_sz(audiod_function_t* audio) {
    if (audio->format_type_tx != AUDIO_FORMAT_TYPE_I) return false;
    if (!audio->n_channels_tx) return false;
    if (!audio->n_bytes_per_sample_tx) return false;
    if (!audio->interval_tx) return false;
    if (!audio->sample_rate_tx) return false;

    // Restart the fractional accumulator for this streaming session.
    audio->tx_sample_acc = 0;

    bool full_speed = backend().isFullSpeed();
    const uint8_t interval =
        full_speed ? audio->interval_tx : 1 << (audio->interval_tx - 1);

    const uint16_t sample_normimal = (uint16_t)(audio->sample_rate_tx *
                                                interval / (full_speed ? 1000 : 8000));
    const uint16_t sample_reminder = (uint16_t)(audio->sample_rate_tx *
                                                interval % (full_speed ? 1000 : 8000));

    const uint16_t packet_sz_tx_min =
        (uint16_t)((sample_normimal - 1) * audio->n_channels_tx *
                   audio->n_bytes_per_sample_tx);
    const uint16_t packet_sz_tx_norm =
        (uint16_t)(sample_normimal * audio->n_channels_tx *
                   audio->n_bytes_per_sample_tx);
    const uint16_t packet_sz_tx_max =
        (uint16_t)((sample_normimal + 1) * audio->n_channels_tx *
                   audio->n_bytes_per_sample_tx);

    // Endpoint size must larger than packet size
    if (packet_sz_tx_max > audio->ep_in_sz) return false;

    // Frmt20.pdf 2.3.1.1 USB Packets
    if (sample_reminder) {
      // All virtual frame packets must either contain INT(nav) audio slots
      // (small VFP) or INT(nav)+1 (large VFP) audio slots
      audio->packet_sz_tx[0] = packet_sz_tx_norm;
      audio->packet_sz_tx[1] = packet_sz_tx_norm;
      audio->packet_sz_tx[2] = packet_sz_tx_max;
    } else {
      // In the case where nav = INT(nav), ni may vary between INT(nav)-1 (small
      // VFP), INT(nav) (medium VFP) and INT(nav)+1 (large VFP).
      audio->packet_sz_tx[0] = packet_sz_tx_min;
      audio->packet_sz_tx[1] = packet_sz_tx_norm;
      audio->packet_sz_tx[2] = packet_sz_tx_max;
    }

    return true;
  }

  // Number of audio bytes to transmit in the current (micro)frame when IN
  // flow control is enabled. A fractional accumulator distributes the
  // sub-frame sample remainder over successive frames so the long-term average
  // matches the configured sample rate (e.g. alternating 176/180 bytes for
  // 44100 Hz stereo 16-bit, averaging 176.4 bytes = 44.1 samples per frame).
  uint16_t audiod_tx_packet_size_fc(audiod_function_t* audio) {
    if (audio->sample_rate_tx == 0 || audio->n_channels_tx == 0 ||
        audio->n_bytes_per_sample_tx == 0) {
      // Not enough info to size precisely: fall back to the max packet.
      return audio->ep_in_sz;
    }
    bool full_speed = backend().isFullSpeed();
    const uint32_t denom = full_speed ? 1000u : 8000u;
    const uint8_t iv = audio->interval_tx ? audio->interval_tx : 1;
    const uint32_t interval = full_speed ? iv : (1u << (iv - 1));

    audio->tx_sample_acc += audio->sample_rate_tx * interval;
    const uint32_t samples = audio->tx_sample_acc / denom;
    audio->tx_sample_acc -= samples * denom;

    uint32_t bytes =
        samples * audio->n_channels_tx * audio->n_bytes_per_sample_tx;
    if (bytes > audio->ep_in_sz) bytes = audio->ep_in_sz;
    return (uint16_t)bytes;
  }

};

}  // namespace audio_tools
