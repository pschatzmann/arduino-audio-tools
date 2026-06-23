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
#ifndef USE_TINYUSB
#error This Microcontroller has no Native USB interface
#endif
#endif

#include "AudioLogger.h"
#include "AudioTools/CoreAudio/BaseStream.h"
#include "USBAudio2DescriptorBuilder.h"

extern "C" {
#include "device/usbd.h"
#include "device/usbd_pvt.h"
#include "tusb.h"
}

#define USB_DESCR_MAX_LEN 512

// TinyUSB >= 0.15 (ESP32 IDF v5+) renamed UAC2 symbols with an AUDIO20_ prefix
// and changed three function signatures.  These shims let the driver build
// against both versions; detect the new API by the presence of the new define.
#ifdef TUD_AUDIO20_DESC_IAD_LEN
#define TUD_AUDIO_DESC_IAD_LEN TUD_AUDIO20_DESC_IAD_LEN
#define AUDIO_CS_AC_INTERFACE_INPUT_TERMINAL \
  AUDIO20_CS_AC_INTERFACE_INPUT_TERMINAL
#define AUDIO_CS_AC_INTERFACE_OUTPUT_TERMINAL \
  AUDIO20_CS_AC_INTERFACE_OUTPUT_TERMINAL
#define AUDIO_CS_CTRL_SAM_FREQ AUDIO20_CS_CTRL_SAM_FREQ
#define AUDIO_CS_CTRL_CLK_VALID AUDIO20_CS_CTRL_CLK_VALID
#define AUDIO_CS_REQ_CUR AUDIO20_CS_REQ_CUR
#define AUDIO_CS_REQ_RANGE AUDIO20_CS_REQ_RANGE
#define AUDIO_CS_AS_INTERFACE_AS_GENERAL AUDIO20_CS_AS_INTERFACE_AS_GENERAL
#define AUDIO_CS_AS_INTERFACE_FORMAT_TYPE AUDIO20_CS_AS_INTERFACE_FORMAT_TYPE
#define audio_desc_cs_ac_interface_t audio20_desc_cs_ac_interface_t
#define audio_desc_cs_as_interface_t audio20_desc_cs_as_interface_t
#define audio_desc_type_I_format_t audio20_desc_type_I_format_t
// New: is_isr parameter added; fifo element-size arg removed
#define TUSB_EDPT_XFER(rp, ep, buf, sz) usbd_edpt_xfer(rp, ep, buf, sz, false)
#define TUSB_EDPT_XFER_FIFO(rp, ep, ff, sz) \
  usbd_edpt_xfer_fifo(rp, ep, ff, sz, false)
#define TUSB_FIFO_CONFIG(f, buf, d, ov) tu_fifo_config(f, buf, d, ov)
#else
// Old TinyUSB (RP2040 / Adafruit bundle)
#define TUSB_EDPT_XFER(rp, ep, buf, sz) usbd_edpt_xfer(rp, ep, buf, sz)
#define TUSB_EDPT_XFER_FIFO(rp, ep, ff, sz) usbd_edpt_xfer_fifo(rp, ep, ff, sz)
#define TUSB_FIFO_CONFIG(f, buf, d, ov) tu_fifo_config(f, buf, d, 1, ov)
// Ensure control selector and request constants are available on old TinyUSB
// too
#ifndef AUDIO10_CS_AC_INTERFACE_INPUT_TERMINAL
#define AUDIO10_CS_AC_INTERFACE_INPUT_TERMINAL \
  AUDIO_CS_AC_INTERFACE_INPUT_TERMINAL
#endif
#ifndef AUDIO_CS_CTRL_CLK_VALID
#define AUDIO_CS_CTRL_CLK_VALID 0x02u
#endif
#ifndef AUDIO_CS_REQ_RANGE
#define AUDIO_CS_REQ_RANGE 0x02u
#endif
#endif

// Feature Unit control selectors (UAC2 Table A-23)
#ifndef AUDIO_FU_CTRL_MUTE
#define AUDIO_FU_CTRL_MUTE 0x01u
#endif
#ifndef AUDIO_FU_CTRL_VOLUME
#define AUDIO_FU_CTRL_VOLUME 0x02u
#endif

namespace audio_tools {

// Discrete sample rates supported by the UAC2 clock source.
static constexpr uint32_t kSupportedSampleRates[] = {
    8000, 11025, 16000, 22050, 24000, 32000, 44100, 48000};
static constexpr uint8_t kNumSupportedSampleRates =
    sizeof(kSupportedSampleRates) / sizeof(kSupportedSampleRates[0]);

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
    // From this point, data is not cleared by bus reset
    uint8_t ctrl_buf_sz;
    tu_fifo_t ep_out_ff;
    tu_fifo_t ep_in_ff;
    std::vector<uint8_t> ctrl_buf;
    std::vector<uint8_t> alt_setting;
    std::vector<uint8_t> lin_buf_out;
    std::vector<uint8_t> lin_buf_in;
    std::vector<uint32_t> fb_buf;
    std::vector<uint8_t> ep_in_sw_buf;   // For feedback EP
    std::vector<uint8_t> ep_out_sw_buf;  // For feedback EP
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

      const uint16_t sw_buf = fifoSize();
      ep_in_sw_buf_sz_.assign(n, sw_buf);
      ep_out_sw_buf_sz_.assign(n, sw_buf);

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

  /** @brief Returns true if FIFO mutex is enabled. */
  bool isFifoMutexEnabled() const { return true; }

  /** @brief Returns the number of audio functions (always 1). */
  uint8_t getAudioCount() const { return 1; }

  /** @brief Returns true if the device is mounted by the USB host. */
  bool mounted() const { return tud_mounted(); }

  /**
   * @brief Register a callback for GET requests on the interface.
   * @param cb Callback function.
   */
  void setGetReqItfCallback(std::function<bool(USBAudioDeviceBase*, uint8_t,
                                               tusb_control_request_t const*)>
                                cb) {
    get_req_itf_cb_ = cb;
  }

  /**
   * @brief Register a callback for GET requests on an endpoint.
   * @param cb Callback function.
   */
  void setGetReqEpCallback(std::function<bool(USBAudioDeviceBase*, uint8_t,
                                              tusb_control_request_t const*)>
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
                         tusb_control_request_t const*)>
          cb) {
    tud_audio_set_itf_cb_ = cb;
  }

  /**
   * @brief Register a callback for entity set requests.
   * @param cb Callback function.
   */
  void setReqEntityCallback(
      std::function<bool(USBAudioDeviceBase*, uint8_t,
                         tusb_control_request_t const*, uint8_t*)>
          cb) {
    tud_audio_set_req_entity_cb_ = cb;
  }

  /**
   * @brief Register a callback for interface set requests.
   * @param cb Callback function.
   */
  void setReqItfCallback(
      std::function<bool(USBAudioDeviceBase*, uint8_t,
                         tusb_control_request_t const*, uint8_t*)>
          cb) {
    tud_audio_set_req_itf_cb_ = cb;
  }

  /**
   * @brief Register a callback for endpoint set requests.
   * @param cb Callback function.
   */
  void setReqEpCallback(
      std::function<bool(USBAudioDeviceBase*, uint8_t,
                         tusb_control_request_t const*, uint8_t*)>
          cb) {
    tud_audio_set_req_ep_cb_ = cb;
  }

  /**
   * @brief Register a callback for interface close endpoint events.
   * @param cb Callback function.
   */
  void setItfCloseEpCallback(std::function<bool(USBAudioDeviceBase*, uint8_t,
                                                tusb_control_request_t const*)>
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

  /** @brief Stop audio streaming and clear FIFOs. Does not disconnect USB. */
  void end() {
    for (auto& audio : audiod_fct_) {
      tu_fifo_clear(&audio.ep_in_ff);
      tu_fifo_clear(&audio.ep_out_ff);
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

  /**
   * @brief Get the USB device class driver for TinyUSB integration.
   * @param count Pointer to store the number of drivers (always 1).
   * @return Pointer to the class driver structure.
   */
  usbd_class_driver_t const* getClassDriver(uint8_t* count) {
    static usbd_class_driver_t driver;
    driver.name = "AUDIO";
    driver.init = [](void) {
      USBAudioDeviceBase::activeInstance().audiod_init();
    };
    driver.deinit = [](void) {
      return USBAudioDeviceBase::activeInstance().audiod_deinit();
    };
    driver.reset = [](uint8_t rhport) {
      USBAudioDeviceBase::activeInstance().audiod_reset(rhport);
    };
    driver.open = [](uint8_t rhport, tusb_desc_interface_t const* itf_desc,
                     uint16_t max_len) {
      return USBAudioDeviceBase::activeInstance().audiod_open(rhport, itf_desc,
                                                              max_len);
    };
    driver.control_xfer_cb = [](uint8_t rhport, uint8_t stage,
                                tusb_control_request_t const* request) {
      return USBAudioDeviceBase::activeInstance().audiod_control_xfer_cb(
          rhport, stage, request);
    };
    driver.xfer_cb = [](uint8_t rhport, uint8_t ep_addr, xfer_result_t result,
                        uint32_t xferred_bytes) {
      return USBAudioDeviceBase::activeInstance().audiod_xfer_cb(
          rhport, ep_addr, result, xferred_bytes);
    };
    driver.sof = [](uint8_t rhport, uint32_t frame_count) {
      USBAudioDeviceBase::activeInstance().audiod_sof_isr(rhport, frame_count);
    };

    *count = 1;
    return &driver;
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
                     tusb_control_request_t const*)>
      get_req_itf_cb_;
  // Callback for endpoint GET requests
  std::function<bool(USBAudioDeviceBase*, uint8_t rhport,
                     tusb_control_request_t const*)>
      get_req_ep_cb_;

  // Callback for feedback done event
  std::function<void(USBAudioDeviceBase*, uint8_t func_id)> fb_done_cb_;
  std::function<bool(USBAudioDeviceBase*, uint8_t func_id)> req_entity_cb_;
  std::function<bool(USBAudioDeviceBase*, uint8_t rhport,
                     tusb_control_request_t const* p_request)>
      tud_audio_set_itf_cb_;
  std::function<bool(USBAudioDeviceBase*, uint8_t rhport,
                     tusb_control_request_t const* p_request, uint8_t* pBuff)>
      tud_audio_set_req_entity_cb_;

  std::function<bool(USBAudioDeviceBase*, uint8_t rhport,
                     tusb_control_request_t const* p_request, uint8_t* pBuff)>
      tud_audio_set_req_itf_cb_;

  std::function<bool(USBAudioDeviceBase*, uint8_t rhport,
                     tusb_control_request_t const* p_request, uint8_t* pBuff)>
      tud_audio_set_req_ep_cb_;

  std::function<bool(USBAudioDeviceBase*, uint8_t rhport,
                     tusb_control_request_t const* p_request)>
      tud_audio_set_itf_close_EP_cb_;

  std::function<void(USBAudioDeviceBase*, uint8_t func_id, uint8_t alt_itf,
                     audio_feedback_params_t* feedback_param)>
      tud_audio_feedback_params_cb_;

  std::function<bool(USBAudioDeviceBase*, uint8_t func_id)>
      tud_audio_feedback_format_correction_cb_;
  uint8_t int_notify_buf_[6] = {};

  std::vector<uint16_t> ep_out_sw_buf_sz_;
  //  (TUD_OPT_HIGH_SPEED ? 32 : 4) * CFG_TUD_AUDIO_EP_SZ_IN // Example write
  //  FIFO every 1ms, so it should be 8 times larger for HS device
  std::vector<uint16_t> ep_in_sw_buf_sz_;
  // calculate!
  std::vector<uint16_t> desc_len_;
  // 64
  std::vector<uint16_t> ctrl_buf_sz_;

  std::vector<audiod_function_t> audiod_fct_;
  // std::vector<osal_mutex_def_t> ep_in_ff_mutex_wr_;
  // std::vector<osal_mutex_def_t> ep_in_ff_mutex_rd_;
  std::vector<osal_mutex_def_t> ep_out_ff_mutex_rd_;

  // s_active_ lets getClassDriver() and the static process() trampoline
  // reach the last-constructed instance without a singleton.
  inline static USBAudioDeviceBase* s_active_ = nullptr;

  /// Set the USB audio configuration (use begin(cfg) instead).
  void setConfig(const USBAudioConfig& cfg) { config_ = cfg; }

  /** @brief Process pending USB events on platforms where the application
   *         drives the stack (RP2040).  No-op on ESP32 where a dedicated
   *         FreeRTOS task calls tud_task() continuously. */
  void serviceTinyUSB() {
#ifdef USE_TINYUSB
    tud_task();
#endif
  }

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
    sendInterruptNotification(AUDIO_CS_CTRL_SAM_FREQ, 0,
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
    if (!tud_mounted()) return;
    for (uint8_t i = 0; i < (uint8_t)audiod_fct_.size(); i++) {
      if (audiod_fct_[i].ep_int == 0) continue;
      int_notify_buf_[0] = 0x00;                // bInfo: interface, not vendor
      int_notify_buf_[1] = AUDIO_CS_REQ_CUR;    // bAttribute: CUR changed
      int_notify_buf_[2] = channel;             // wValue low = CN
      int_notify_buf_[3] = ctrlSel;             // wValue high = CS
      int_notify_buf_[4] = config_.itf_num_ac;  // wIndex low = interface
      int_notify_buf_[5] = entityID;            // wIndex high = entity ID
      (void)TUSB_EDPT_XFER(0, audiod_fct_[i].ep_int, int_notify_buf_, 6);
      break;
    }
  }

  bool configChanged(const USBAudioConfig& n) { return config_ != n; }

  // Returns the control buffer size for a given function number
  uint16_t getCtrlBufSz(uint8_t fn) const {
    return (fn < ctrl_buf_sz_.size()) ? ctrl_buf_sz_[fn] : 64;
  }

  // Returns the OUT software buffer size for a given function number
  uint16_t getEpOutSwBufSz(uint8_t fn) const {
    return (fn < ep_out_sw_buf_sz_.size()) ? ep_out_sw_buf_sz_[fn] : 0;
  }

  // Returns the IN software buffer size for a given function number
  uint16_t getEpInSwBufSz(uint8_t fn) const {
    return (fn < ep_in_sw_buf_sz_.size()) ? ep_in_sw_buf_sz_[fn] : 0;
  }

  // Returns the descriptor length for a given function number
  uint16_t getDescLen(uint8_t fn) const {
    return (fn < desc_len_.size()) ? desc_len_[fn] : 0;
  }

  bool isUseLinearBufferRx() const { return config_.use_linear_buffer_rx; }

  bool isUseLinearBufferTx() const { return config_.use_linear_buffer_tx; }

  static bool isValidBitsPerSample(uint8_t bps) {
    return bps == 16 || bps == 24 || bps == 32;
  }

  void notifyStreamingState() {
    if (streaming_state_cb_)
      streaming_state_cb_(isStreamingActiveTx(), isStreamingActiveRx());
  }

  // Max Bytes for one 1 ms isochronous USB packet.
  uint16_t packetSize() const { return descr_builder.calcMaxPacketSize(); }

  // Total audio FIFO size in bytes.
  uint16_t fifoSize() const {
    uint16_t sz = (uint16_t)(packetSize() * config_.fifo_packets);
    // // Round up to next power of 2 — some tu_fifo implementations use
    // // idx & (depth-1) for index wrapping, which requires power-of-2 depth.
    uint16_t p = 256;
    while (p < sz) p <<= 1;
    return p;
  }

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
        uint32_t ff_count = tu_fifo_count(&audio->ep_out_ff);
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
        audio->feedback.value =
            TU_MIN(TU_MAX(audio->feedback.value, audio->feedback.min_value),
                   audio->feedback.max_value);
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
        uint32_t frame_div =
            (TUSB_SPEED_FULL == tud_speed_get()) ? 1000u : 8000u;
        audio->feedback.value =
            (audio->feedback.compute.fixed.sample_freq << 16) / frame_div;
      } break;

      default:
        break;
    }

    if (usbd_edpt_claim(audio->rhport, audio->ep_fb)) {
      audiod_fb_send(audio);
    }
  }

  // USBD Driver API
  void audiod_init(void) {
    audiod_fct_.resize(getAudioCount());
    alloc_mutex();

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
        // Max packet across all supported rates (192 kHz):
        uint16_t max_pkt = descr_builder.calcPacketSizeForRate(192000);
        audio->lin_buf_in.resize(max_pkt);
      }
      // Initialize OUT EP — always set up both FIFO and linear buffer.
      // The FIFO is needed by TinyUSB internals even in linear buffer mode.
      if (isEpOutEnabled()) {
        audio->ep_out_sw_buf.resize(getEpOutSwBufSz(i));
        TUSB_FIFO_CONFIG(&audio->ep_out_ff, audio->ep_out_sw_buf.data(),
                         getEpOutSwBufSz(i), true);
        if (isFifoMutexEnabled()) {
          tu_fifo_config_mutex(&audio->ep_out_ff, NULL,
                               osal_mutex_create(&ep_out_ff_mutex_rd_[i]));
        }
        uint16_t max_pkt = descr_builder.calcPacketSizeForRate(192000);
        audio->lin_buf_out.resize(max_pkt);
      }
      if (isFeedbackEpEnabled()) {
        audio->fb_buf.resize(1);  // one uint32_t = 4 bytes of feedback data
      }
    }
  }

  void alloc_mutex() {
    if (isFifoMutexEnabled()) {
      if (isEpOutEnabled()) {
        ep_out_ff_mutex_rd_.resize(getAudioCount());
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
        tu_fifo_clear(&audio->ep_in_ff);
        bufferTx().reset();
      }
      if (isEpOutEnabled()) {
        tu_fifo_clear(&audio->ep_out_ff);
        bufferRx().reset();
      }
    }
  }

  uint16_t audiod_open(uint8_t rhport, tusb_desc_interface_t const* itf_desc,
                       uint16_t max_len) {
    (void)max_len;
    TU_VERIFY(TUSB_CLASS_AUDIO == itf_desc->bInterfaceClass &&
              AUDIO_SUBCLASS_CONTROL == itf_desc->bInterfaceSubClass);
    TU_VERIFY(itf_desc->bInterfaceProtocol == AUDIO_INT_PROTOCOL_CODE_V2);
    TU_ASSERT(itf_desc->bNumEndpoints <= 1);
    if (itf_desc->bNumEndpoints == 1) {
      TU_ASSERT(isInterruptEpEnabled());
    }
    TU_VERIFY(itf_desc->bAlternateSetting == 0);
    uint8_t i;
    for (i = 0; i < getAudioCount(); i++) {
      if (!audiod_fct_[i].p_desc) {
        audiod_fct_[i].p_desc = (uint8_t const*)itf_desc;
        audiod_fct_[i].rhport = rhport;
        audiod_fct_[i].desc_length = getDescLen(i);
        // audiod_reset() zeroes ctrl_buf_sz via memset — restore it so
        // tud_control_xfer() receives the correct buffer length.
        audiod_fct_[i].ctrl_buf_sz = getCtrlBufSz(i);
        if (isEpInEnabled() || isEpOutEnabled() || isFeedbackEpEnabled()) {
          uint8_t ep_in = 0, ep_out = 0, ep_fb = 0;
          uint16_t ep_in_size = 0, ep_out_size = 0;
          tusb_desc_endpoint_t const* desc_ep_out = nullptr;
          uint8_t const* p_desc = audiod_fct_[i].p_desc;
          uint8_t const* p_desc_end =
              p_desc + audiod_fct_[i].desc_length - TUD_AUDIO_DESC_IAD_LEN;
          while (p_desc_end - p_desc > 0) {
            if (tu_desc_type(p_desc) == TUSB_DESC_ENDPOINT) {
              tusb_desc_endpoint_t const* desc_ep =
                  (tusb_desc_endpoint_t const*)p_desc;
              if (desc_ep->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS) {
                if (isFeedbackEpEnabled() && desc_ep->bmAttributes.usage == 1) {
                  ep_fb = desc_ep->bEndpointAddress;
                }
                if (desc_ep->bmAttributes.usage == 0) {
                  if (isEpInEnabled() &&
                      tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN) {
                    ep_in = desc_ep->bEndpointAddress;
                    ep_in_size =
                        TU_MAX(tu_edpt_packet_size(desc_ep), ep_in_size);
                  } else if (isEpOutEnabled() &&
                             tu_edpt_dir(desc_ep->bEndpointAddress) ==
                                 TUSB_DIR_OUT) {
                    ep_out = desc_ep->bEndpointAddress;
                    ep_out_size =
                        TU_MAX(tu_edpt_packet_size(desc_ep), ep_out_size);
                    desc_ep_out = desc_ep;
                  }
                }
              }
            }
            p_desc = tu_desc_next(p_desc);
          }
          if (isEpInEnabled() && ep_in) {
            bool alloc_ok = usbd_edpt_iso_alloc(rhport, ep_in, ep_in_size);
            LOGD("iso_alloc IN ep=0x%02x sz=%u: %s", ep_in, ep_in_size,
                 alloc_ok ? "OK" : "FAIL");
          }
          if (isEpOutEnabled() && ep_out) {
            bool alloc_ok = usbd_edpt_iso_alloc(rhport, ep_out, ep_out_size);
            LOGD("iso_alloc OUT ep=0x%02x sz=%u: %s", ep_out, ep_out_size,
                 alloc_ok ? "OK" : "FAIL");
#ifdef TUP_DCD_EDPT_ISO_ALLOC
            // Pre-activate during enumeration (no isochronous traffic).
            // Cannot be done in SET_INTERFACE because iso_activate blocks
            // on ESP32's DWC2 when the host is already sending.
            // release clears the busy flag so XFER can arm later.
            if (desc_ep_out) {
              usbd_edpt_iso_activate(rhport, desc_ep_out);
              usbd_edpt_release(rhport, ep_out);
              LOGD("iso_activate+release OUT: done");
            }
#endif
          }
          if (isFeedbackEpEnabled() && ep_fb) {
            usbd_edpt_iso_alloc(rhport, ep_fb, 4);
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
              p_desc + audiod_fct_[i].desc_length - TUD_AUDIO_DESC_IAD_LEN;
          while (p_desc_end - p_desc > 0) {
            if (tu_desc_type(p_desc) == TUSB_DESC_ENDPOINT) {
              if (isEpInEnabled()) {
                tusb_desc_endpoint_t const* desc_ep =
                    (tusb_desc_endpoint_t const*)p_desc;
                if (desc_ep->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS &&
                    desc_ep->bmAttributes.usage == 0 &&
                    tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN) {
                  audiod_fct_[i].interval_tx = desc_ep->bInterval;
                }
              }
            } else if (tu_desc_type(p_desc) == TUSB_DESC_CS_INTERFACE) {
              if (tu_desc_subtype(p_desc) ==
                  AUDIO_CS_AC_INTERFACE_OUTPUT_TERMINAL) {
                if (tu_unaligned_read16(p_desc + 4) ==
                    AUDIO_TERM_TYPE_USB_STREAMING) {
                  audiod_fct_[i].bclock_id_tx = p_desc[8];  // OT bCSourceID
                }
              } else if (tu_desc_subtype(p_desc) ==
                         AUDIO_CS_AC_INTERFACE_INPUT_TERMINAL) {
                if (tu_unaligned_read16(p_desc + 4) ==
                    AUDIO_TERM_TYPE_USB_STREAMING) {
                  audiod_fct_[i].bclock_id_tx = p_desc[7];  // IT bCSourceID
                }
              }
            }
            p_desc = tu_desc_next(p_desc);
          }
        }

        if (isInterruptEpEnabled()) {
          uint8_t const* p_desc = audiod_fct_[i].p_desc;
          uint8_t const* p_desc_end =
              p_desc + audiod_fct_[i].desc_length - TUD_AUDIO_DESC_IAD_LEN;
          while (p_desc_end - p_desc > 0) {
            if (tu_desc_type(p_desc) == TUSB_DESC_ENDPOINT) {
              tusb_desc_endpoint_t const* desc_ep =
                  (tusb_desc_endpoint_t const*)p_desc;
              uint8_t const ep_addr = desc_ep->bEndpointAddress;
              if (tu_edpt_dir(ep_addr) == TUSB_DIR_IN &&
                  desc_ep->bmAttributes.xfer == TUSB_XFER_INTERRUPT) {
                if (usbd_edpt_open(audiod_fct_[i].rhport, desc_ep)) {
                  audiod_fct_[i].ep_int = ep_addr;
                } else {
                  LOGE("  UAC2: interrupt EP 0x%02x open failed", ep_addr);
                        
                }
              }
            }
            p_desc = tu_desc_next(p_desc);
          }
        }
        audiod_fct_[i].mounted = true;
        break;
      }
    }
    TU_ASSERT(i < getAudioCount());
    uint16_t drv_len = audiod_fct_[i].desc_length - TUD_AUDIO_DESC_IAD_LEN;
    return drv_len;
  }

  bool audiod_control_xfer_cb(uint8_t rhport, uint8_t stage,
                              tusb_control_request_t const* request) {
    if (stage == CONTROL_STAGE_SETUP) {
      return audiod_control_request(rhport, request);
    } else if (stage == CONTROL_STAGE_DATA) {
      return audiod_control_complete(rhport, request);
    }
    return true;
  }
  // Invoked when class request DATA stage is finished.
  // return false to stall control EP (e.g Host send non-sense DATA)
  bool audiod_control_complete(uint8_t rhport,
                               tusb_control_request_t const* p_request) {
    // Handle audio class specific set requests
    if (p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS &&
        p_request->bmRequestType_bit.direction == TUSB_DIR_OUT) {
      uint8_t func_id;

      switch (p_request->bmRequestType_bit.recipient) {
        case TUSB_REQ_RCPT_INTERFACE: {
          uint8_t itf = TU_U16_LOW(p_request->wIndex);
          uint8_t entityID = TU_U16_HIGH(p_request->wIndex);

          if (entityID != 0) {
            func_id = 0;
            uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
            uint8_t* cb = audiod_fct_[func_id].ctrl_buf.data();

            // ── Clock Source SET_CUR (sample rate) ──────────────
            if (entityID == USBAudio2DescriptorBuilder::ENTITY_CLOCK &&
                ctrlSel == AUDIO_CS_CTRL_SAM_FREQ &&
                p_request->bRequest == AUDIO_CS_REQ_CUR) {
              setSampleRate(tu_unaligned_read32(cb));
            }

            // ── Feature Unit SET_CUR (mute / volume) ────────────
            if (isFeatureUnit(entityID) &&
                p_request->bRequest == AUDIO_CS_REQ_CUR) {
              uint8_t channel = TU_U16_LOW(p_request->wValue);
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
            TU_VERIFY(audiod_verify_itf_exists(itf, &func_id));

            // Invoke callback
            if (tud_audio_set_req_itf_cb_) {
              return tud_audio_set_req_itf_cb_(
                  this, rhport, p_request,
                  audiod_fct_[func_id].ctrl_buf.data());
            }
          }
        } break;

        case TUSB_REQ_RCPT_ENDPOINT: {
          uint8_t ep = TU_U16_LOW(p_request->wIndex);

          // Check if entity is present and get corresponding driver index
          TU_VERIFY(audiod_verify_ep_exists(ep, &func_id));

          // Invoke callback
          if (tud_audio_set_req_ep_cb_) {
            return tud_audio_set_req_ep_cb_(
                this, rhport, p_request, audiod_fct_[func_id].ctrl_buf.data());
          }
        } break;
        // Unknown/Unsupported recipient
        default:
          TU_BREAKPOINT();
          return false;
      }
    }
    return true;
  }

  /// TODO refactor control request handling to separate function and reduce nesting
  bool audiod_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result,
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
          (void)TUSB_EDPT_XFER(rhport, audio->ep_in, dst, frame_bytes);
        }
        return true;
      }
      if (isEpOutEnabled() && audio->ep_out == ep_addr) {
        xfer_cb_rx_count_ = xfer_cb_rx_count_ + 1;
        rx_total_bytes_ += xferred_bytes;
        if (isUseLinearBufferRx()) {
          // Copy DMA-received data into the platform buffer, re-arm DMA.
          if (xferred_bytes > 0)
            bufferRx().writeArray(audio->lin_buf_out.data(),
                                  (int)xferred_bytes);
          if (rx_done_cb_)
            rx_done_cb_(this, rhport, audio, (uint16_t)xferred_bytes);
          (void)TUSB_EDPT_XFER(rhport, audio->ep_out, audio->lin_buf_out.data(),
                               audio->ep_out_sz);
        } else {
          // FIFO mode: data is already in ep_out_ff from DMA, copy to buffer
          if (xferred_bytes > 0) {
            uint8_t tmp[768];
            uint16_t n =
                tu_fifo_read_n(&audio->ep_out_ff, tmp, (uint16_t)xferred_bytes);
            if (n > 0) bufferRx().writeArray(tmp, n);
          }
          if (rx_done_cb_)
            rx_done_cb_(this, rhport, audio, (uint16_t)xferred_bytes);
          (void)TUSB_EDPT_XFER_FIFO(rhport, audio->ep_out, &audio->ep_out_ff,
                                    audio->ep_out_sz);
        }
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

  TU_ATTR_FAST_FUNC void audiod_sof_isr(uint8_t rhport, uint32_t frame_count) {
    (void)rhport;
    (void)frame_count;
    if (isEpOutEnabled() && isFeedbackEpEnabled()) {
      for (uint8_t i = 0; i < getAudioCount(); i++) {
        audiod_function_t* audio = &audiod_fct_[i];
        if (audio->ep_fb != 0) {
          uint8_t const hs_adjust =
              (TUSB_SPEED_HIGH == tud_speed_get()) ? 3 : 0;
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

  // ── Clock Source GET handler ────────────────────────────────────────────
  bool handleClockSourceGet(uint8_t rhport,
                            tusb_control_request_t const* p_request,
                            uint8_t* cb) {
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    if (ctrlSel == AUDIO_CS_CTRL_CLK_VALID &&
        p_request->bRequest == AUDIO_CS_REQ_CUR) {
      cb[0] = 1;
      return tud_control_xfer(rhport, p_request, cb, 1);
    }
    if (ctrlSel == AUDIO_CS_CTRL_SAM_FREQ) {
      uint32_t rate = (uint32_t)config_.sample_rate;
      if (p_request->bRequest == AUDIO_CS_REQ_CUR) {
        memcpy(cb, &rate, 4);
        return tud_control_xfer(rhport, p_request, cb, 4);
      }
      if (p_request->bRequest == AUDIO_CS_REQ_RANGE) {
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
          return tud_control_xfer(rhport, p_request, cb,
                                  (uint16_t)(2 + kNumSupportedSampleRates * 12));
        } else {
          // Single fixed rate from config
          uint16_t cnt = 1;
          uint32_t z = 0;
          memcpy(cb,      &cnt,  2);
          memcpy(cb + 2,  &rate, 4);  // dMIN
          memcpy(cb + 6,  &rate, 4);  // dMAX
          memcpy(cb + 10, &z,    4);  // dRES = 0 (fixed)
          return tud_control_xfer(rhport, p_request, cb, 14);
        }
      }
    }
    return false;
  }

  // ── Feature Unit GET handler ──────────────────────────────────────────
  bool handleFeatureUnitGet(uint8_t rhport,
                            tusb_control_request_t const* p_request,
                            uint8_t* cb) {
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    uint8_t channel = TU_U16_LOW(p_request->wValue);
    if (ctrlSel == AUDIO_FU_CTRL_MUTE &&
        p_request->bRequest == AUDIO_CS_REQ_CUR) {
      cb[0] = isMute(channel) ? 1 : 0;
      return tud_control_xfer(rhport, p_request, cb, 1);
    }
    if (ctrlSel == AUDIO_FU_CTRL_VOLUME) {
      if (p_request->bRequest == AUDIO_CS_REQ_CUR) {
        int16_t v = floatToUac2(volume(channel));
        memcpy(cb, &v, 2);
        return tud_control_xfer(rhport, p_request, cb, 2);
      }
      if (p_request->bRequest == AUDIO_CS_REQ_RANGE) {
        uint16_t cnt = 1;
        int16_t vmin = -25600, vmax = 0, vres = 256;
        memcpy(cb + 0, &cnt, 2);
        memcpy(cb + 2, &vmin, 2);
        memcpy(cb + 4, &vmax, 2);
        memcpy(cb + 6, &vres, 2);
        return tud_control_xfer(rhport, p_request, cb, 8);
      }
    }
    return false;
  }

  // ── Entity request handler (Clock Source + Feature Unit) ──────────────
  bool handleEntityRequest(uint8_t rhport,
                           tusb_control_request_t const* p_request,
                           uint8_t entityID) {
    uint8_t func_id = 0;
    uint8_t* cb = audiod_fct_[func_id].ctrl_buf.data();
    bool is_get = (p_request->bmRequestType_bit.direction == TUSB_DIR_IN);

    if (entityID == USBAudio2DescriptorBuilder::ENTITY_CLOCK) {
      if (is_get && handleClockSourceGet(rhport, p_request, cb)) return true;
      // SET — schedule data receive for audiod_control_complete()
      return tud_control_xfer(rhport, p_request, cb,
                              audiod_fct_[func_id].ctrl_buf_sz);
    }

    if (isFeatureUnit(entityID)) {
      if (is_get && handleFeatureUnitGet(rhport, p_request, cb)) return true;
      // SET — schedule data receive
      return tud_control_xfer(rhport, p_request, cb,
                              audiod_fct_[func_id].ctrl_buf_sz);
    }

    // Unknown entity — try generic verify
    uint8_t itf = TU_U16_LOW(p_request->wIndex);
    if (!audiod_verify_entity_exists(itf, entityID, &func_id)) {
      tud_control_status(rhport, p_request);
      return true;
    }
    if (is_get && req_entity_cb_) return req_entity_cb_(this, func_id);
    return tud_control_xfer(rhport, p_request,
                            audiod_fct_[func_id].ctrl_buf.data(),
                            audiod_fct_[func_id].ctrl_buf_sz);
  }

  // ── Interface request handler (entityID == 0) ─────────────────────────
  bool handleInterfaceRequest(uint8_t rhport,
                              tusb_control_request_t const* p_request) {
    uint8_t itf = TU_U16_LOW(p_request->wIndex);
    uint8_t func_id;
    TU_VERIFY(audiod_verify_itf_exists(itf, &func_id));
    if (p_request->bmRequestType_bit.direction == TUSB_DIR_IN) {
      if (get_req_itf_cb_) return get_req_itf_cb_(this, rhport, p_request);
      return false;
    }
    return tud_control_xfer(rhport, p_request,
                            audiod_fct_[func_id].ctrl_buf.data(),
                            audiod_fct_[func_id].ctrl_buf_sz);
  }

  // ── Endpoint request handler ──────────────────────────────────────────
  bool handleEndpointRequest(uint8_t rhport,
                             tusb_control_request_t const* p_request) {
    uint8_t ep = TU_U16_LOW(p_request->wIndex);
    uint8_t func_id;
    TU_VERIFY(audiod_verify_ep_exists(ep, &func_id));
    if (p_request->bmRequestType_bit.direction == TUSB_DIR_IN) {
      if (get_req_ep_cb_) return get_req_ep_cb_(this, rhport, p_request);
      return false;
    }
    return tud_control_xfer(rhport, p_request,
                            audiod_fct_[func_id].ctrl_buf.data(),
                            audiod_fct_[func_id].ctrl_buf_sz);
  }

  // ── Main control request dispatcher ───────────────────────────────────
  bool audiod_control_request(uint8_t rhport,
                              tusb_control_request_t const* p_request) {
    if (p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD) {
      switch (p_request->bRequest) {
        case TUSB_REQ_GET_INTERFACE:
          return audiod_get_interface(rhport, p_request);
        case TUSB_REQ_SET_INTERFACE:
          return audiod_set_interface(rhport, p_request);
        case TUSB_REQ_CLEAR_FEATURE:
          return true;
        default:
          return false;
      }
    }

    if (p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS) {
      switch (p_request->bmRequestType_bit.recipient) {
        case TUSB_REQ_RCPT_INTERFACE: {
          uint8_t entityID = TU_U16_HIGH(p_request->wIndex);
          return (entityID != 0)
                     ? handleEntityRequest(rhport, p_request, entityID)
                     : handleInterfaceRequest(rhport, p_request);
        }
        case TUSB_REQ_RCPT_ENDPOINT:
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
          ((tusb_desc_interface_t const*)audiod_fct_[i].p_desc)
                  ->bInterfaceNumber == itf) {
        // Get pointers after class specific AC descriptors and end of AC
        // descriptors - entities are defined in between
        uint8_t const* p_desc =
            tu_desc_next(audiod_fct_[i].p_desc);  // Points to CS AC descriptor
        uint8_t const* p_desc_end =
            ((audio_desc_cs_ac_interface_t const*)p_desc)->wTotalLength +
            p_desc;
        p_desc = tu_desc_next(p_desc);  // Get past CS AC descriptor

        // Condition modified from p_desc < p_desc_end to prevent gcc>=12
        // strict-overflow warning
        while (p_desc_end - p_desc > 0) {
          if (p_desc[3] == entityID)  // Entity IDs are always at offset 3
          {
            *func_id = i;
            return true;
          }
          p_desc = tu_desc_next(p_desc);
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
        uint8_t const* p_desc = tu_desc_next(audiod_fct_[i].p_desc);
        p_desc += ((audio_desc_cs_ac_interface_t const*)p_desc)->wTotalLength;

        // Condition modified from p_desc < p_desc_end to prevent gcc>=12
        // strict-overflow warning
        while (p_desc_end - p_desc > 0) {
          if (tu_desc_type(p_desc) == TUSB_DESC_ENDPOINT &&
              ((tusb_desc_endpoint_t const*)p_desc)->bEndpointAddress == ep) {
            *func_id = i;
            return true;
          }
          p_desc = tu_desc_next(p_desc);
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
                                    TUD_AUDIO_DESC_IAD_LEN;
        // Condition modified from p_desc < p_desc_end to prevent gcc>=12
        // strict-overflow warning
        while (p_desc_end - p_desc > 0) {
          if (tu_desc_type(p_desc) == TUSB_DESC_INTERFACE &&
              ((tusb_desc_interface_t const*)audiod_fct_[i].p_desc)
                      ->bInterfaceNumber == itf) {
            *func_id = i;
            return true;
          }
          p_desc = tu_desc_next(p_desc);
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

    p_desc = tu_desc_next(p_desc);  // Exclude standard AS interface descriptor
                                    // of current alternate interface descriptor

    // Look for a Class-Specific AS Interface Descriptor(4.9.2) to verify format
    // type and format and also to get number of physical channels
    if (tu_desc_type(p_desc) == TUSB_DESC_CS_INTERFACE &&
        tu_desc_subtype(p_desc) == AUDIO_CS_AS_INTERFACE_AS_GENERAL) {
      audio->n_channels_tx =
          ((audio_desc_cs_as_interface_t const*)p_desc)->bNrChannels;
      audio->format_type_tx =
          (audio_format_type_t)(((audio_desc_cs_as_interface_t const*)p_desc)
                                    ->bFormatType);
      // Look for a Type I Format Type Descriptor(2.3.1.6 - Audio Formats)
      p_desc = tu_desc_next(p_desc);
      if (tu_desc_type(p_desc) == TUSB_DESC_CS_INTERFACE &&
          tu_desc_subtype(p_desc) == AUDIO_CS_AS_INTERFACE_FORMAT_TYPE &&
          ((audio_desc_type_I_format_t const*)p_desc)->bFormatType ==
              AUDIO_FORMAT_TYPE_I) {
        audio->n_bytes_per_sample_tx =
            ((audio_desc_type_I_format_t const*)p_desc)->bSubslotSize;
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
          audio->p_desc + audio->desc_length - TUD_AUDIO_DESC_IAD_LEN;

      // Advance past AC descriptors
      uint8_t const* p_desc = tu_desc_next(audio->p_desc);
      p_desc += ((audio_desc_cs_ac_interface_t const*)p_desc)->wTotalLength;

      uint8_t tmp = 0;
      // Condition modified from p_desc < p_desc_end to prevent gcc>=12
      // strict-overflow warning
      while (p_desc_end - p_desc > 0) {
        // We assume the number of alternate settings is increasing thus we
        // return the index of alternate setting zero!
        if (tu_desc_type(p_desc) == TUSB_DESC_INTERFACE &&
            ((tusb_desc_interface_t const*)p_desc)->bAlternateSetting == 0) {
          if (((tusb_desc_interface_t const*)p_desc)->bInterfaceNumber == itf) {
            *idxItf = tmp;
            *pp_desc_int = p_desc;
            return true;
          }
          // Increase index, bytes read, and pointer
          tmp++;
        }
        p_desc = tu_desc_next(p_desc);
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
                            tusb_control_request_t const* p_request) {
    uint8_t const itf = tu_u16_low(p_request->wIndex);

    // Find index of audio streaming interface
    uint8_t func_id, idxItf;
    uint8_t const* dummy;

    TU_VERIFY(
        audiod_get_AS_interface_index_global(itf, &func_id, &idxItf, &dummy));
    TU_VERIFY(tud_control_xfer(rhport, p_request,
                               &audiod_fct_[func_id].alt_setting[idxItf], 1));

    LOGI("  Get itf: %u - current alt: %u", itf,
            audiod_fct_[func_id].alt_setting[idxItf]);

    return true;
  }

  bool audiod_fb_send(audiod_function_t* audio) {
    bool apply_correction = (TUSB_SPEED_FULL == tud_speed_get()) &&
                            audio->feedback.format_correction;
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
    return TUSB_EDPT_XFER(audio->rhport, audio->ep_fb,
                          (uint8_t*)audio->fb_buf.data(),
                          apply_correction ? 3 : 4);
  }

  // ── Close existing EPs for this interface ──────────────────────────────
  void closeEpIn(uint8_t rhport, audiod_function_t* audio, uint8_t itf,
                 tusb_control_request_t const* p_request) {
    if (!isEpInEnabled() || audio->ep_in_as_intf_num != itf) return;
    audio->ep_in_as_intf_num = 0;
#ifndef TUP_DCD_EDPT_ISO_ALLOC
    usbd_edpt_close(rhport, audio->ep_in);
#endif
    tu_fifo_clear(&audio->ep_in_ff);
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
                  tusb_control_request_t const* p_request) {
    if (!isEpOutEnabled() || audio->ep_out_as_intf_num != itf) return;
    audio->ep_out_as_intf_num = 0;
#ifndef TUP_DCD_EDPT_ISO_ALLOC
    usbd_edpt_close(rhport, audio->ep_out);
#endif
    tu_fifo_clear(&audio->ep_out_ff);
    if (tud_audio_set_itf_close_EP_cb_)
      tud_audio_set_itf_close_EP_cb_(this, rhport, p_request);
    audio->ep_out = 0;
    if (isFeedbackEpEnabled()) {
      audio->ep_fb = 0;
      tu_memclr(&audio->feedback, sizeof(audio->feedback));
    }
    notifyStreamingState();
  }

  // ── Activate a single endpoint found in the descriptor ────────────────
  bool activateEndpoint(uint8_t rhport, tusb_desc_endpoint_t const* desc_ep,
                        uint8_t dir = TUSB_DIR_IN) {
#ifdef TUP_DCD_EDPT_ISO_ALLOC
    // Skip iso_activate for isochronous OUT — on ESP32's DWC2 it blocks
    // for the entire playback duration.  The endpoint DPRAM was already
    // allocated by iso_alloc in audiod_open().  The XFER call in
    // openEpOut will configure the DCD to receive.
    if (dir == TUSB_DIR_OUT &&
        desc_ep->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS)
      return true;
    return usbd_edpt_iso_activate(rhport, desc_ep);
#else
    (void)dir;
    return usbd_edpt_open(rhport, desc_ep);
#endif
  }

  // ── Open the IN (TX) data endpoint ────────────────────────────────────
  void openEpIn(uint8_t rhport, audiod_function_t* audio, uint8_t itf,
                tusb_desc_endpoint_t const* desc_ep,
                uint8_t const* p_desc_for_params) {
    audio->ep_in = desc_ep->bEndpointAddress;
    audio->ep_in_as_intf_num = itf;
    audio->ep_in_sz = tu_edpt_packet_size(desc_ep);
    if (audio->ep_in_sz == 0) return;

    if (isEpInFlowControlEnabled())
      audiod_parse_flow_control_params(audio, p_desc_for_params);

    // Arm initial transfer (silence — copier fills the buffer).
    uint16_t first_pkt = packetSize();
    if (first_pkt > audio->ep_in_sz) first_pkt = audio->ep_in_sz;
    audio->lin_buf_in.assign(audio->ep_in_sz, 0);
    tx_xfer_armed_ = TUSB_EDPT_XFER(rhport, audio->ep_in,
                                    audio->lin_buf_in.data(), first_pkt);
    notifyStreamingState();
  }

  // ── Open the OUT (RX) data endpoint ───────────────────────────────────
  void openEpOut(uint8_t rhport, audiod_function_t* audio, uint8_t itf,
                 tusb_desc_endpoint_t const* desc_ep) {
    LOGD("openEpOut: ep=0x%02x sz=%u", desc_ep->bEndpointAddress,
         tu_edpt_packet_size(desc_ep));
    audio->ep_out = desc_ep->bEndpointAddress;
    audio->ep_out_as_intf_num = itf;
    audio->ep_out_sz = tu_edpt_packet_size(desc_ep);
    if (audio->ep_out_sz == 0) return;

    // iso_activate was done in audiod_open() (no traffic, instant).
    // Just arm the XFER here.
    if (isUseLinearBufferRx()) {
      if (audio->lin_buf_out.size() < audio->ep_out_sz)
        audio->lin_buf_out.assign(audio->ep_out_sz, 0);
      bool xfer_ok = TUSB_EDPT_XFER(rhport, audio->ep_out,
                                    audio->lin_buf_out.data(),
                                    audio->ep_out_sz);
      LOGD("  XFER armed: %s, buf=%p sz=%u", xfer_ok ? "OK" : "FAIL",
           audio->lin_buf_out.data(), audio->ep_out_sz);
    } else {
      bool xfer_ok = TUSB_EDPT_XFER_FIFO(rhport, audio->ep_out,
                                          &audio->ep_out_ff,
                                          audio->ep_out_sz);
      LOGD("  XFER_FIFO armed: %s", xfer_ok ? "OK" : "FAIL");
    }
    notifyStreamingState();
  }

  // ── Open the explicit feedback endpoint ───────────────────────────────
  void openEpFeedback(audiod_function_t* audio,
                      tusb_desc_endpoint_t const* desc_ep) {
    audio->ep_fb = desc_ep->bEndpointAddress;
    audio->feedback.frame_shift = desc_ep->bInterval - 1;
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

    if (TUSB_SPEED_FULL == tud_speed_get() &&
        tud_audio_feedback_format_correction_cb_)
      audio->feedback.format_correction =
          tud_audio_feedback_format_correction_cb_(this, func_id);

    uint32_t const frame_div =
        (TUSB_SPEED_FULL == tud_speed_get()) ? 1000 : 8000;
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
        audio->feedback.compute.fifo_count.fifo_lvl_avg =
            ((uint32_t)fifo_lvl_thr) << 16;
        uint32_t nominal =
            ((fb_param.sample_freq / 100) << 16) / (frame_div / 100);
        audio->feedback.compute.fifo_count.nom_value = nominal;
        audio->feedback.compute.fifo_count.rate_const[0] =
            (uint16_t)((audio->feedback.max_value - nominal) / fifo_lvl_thr);
        audio->feedback.compute.fifo_count.rate_const[1] =
            (uint16_t)((nominal - audio->feedback.min_value) / fifo_lvl_thr);
        if (tud_speed_get() == TUSB_SPEED_HIGH) {
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
        p_desc + audio->desc_length - TUD_AUDIO_DESC_IAD_LEN;
    LOGD("  openEPs: p_desc=%p end=%p len=%u itf=%u alt=%u",
         p_desc, p_desc_end, audio->desc_length, itf, alt);

    while (p_desc_end - p_desc > 0) {
      if (tu_desc_type(p_desc) == TUSB_DESC_INTERFACE &&
          ((tusb_desc_interface_t const*)p_desc)->bInterfaceNumber == itf &&
          ((tusb_desc_interface_t const*)p_desc)->bAlternateSetting == alt) {
        uint8_t const* p_desc_for_params =
            (isEpInEnabled() && isEpInFlowControlEnabled()) ? p_desc : nullptr;
        uint8_t foundEPs = 0;
        uint8_t nEps = ((tusb_desc_interface_t const*)p_desc)->bNumEndpoints;
        LOGD("  matched itf=%u alt=%u nEps=%u", itf, alt, nEps);

        while (foundEPs < nEps && (p_desc_end - p_desc > 0)) {
          LOGD("  scan: type=0x%02x len=%u offset=%d",
               p_desc[1], p_desc[0], (int)(p_desc - audio->p_desc));
          if (tu_desc_type(p_desc) == TUSB_DESC_ENDPOINT) {
            tusb_desc_endpoint_t const* desc_ep =
                (tusb_desc_endpoint_t const*)p_desc;

            LOGD("  activating ep=0x%02x type=%u...",
                 desc_ep->bEndpointAddress, desc_ep->bmAttributes.xfer);
            if (!activateEndpoint(rhport, desc_ep,
                                  tu_edpt_dir(desc_ep->bEndpointAddress))) {
              LOGD("  activateEndpoint FAILED");
              p_desc = tu_desc_next(p_desc);
              continue;
            }
            LOGD("  activated OK");
            // Skip clear_stall for isochronous OUT (iso_activate was also
            // skipped).  For other endpoints, clear the stall as usual.
            if (!(tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_OUT &&
                  desc_ep->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS))
              usbd_edpt_clear_stall(rhport, desc_ep->bEndpointAddress);

            uint8_t ep_addr = desc_ep->bEndpointAddress;
            if (isEpInEnabled() && tu_edpt_dir(ep_addr) == TUSB_DIR_IN &&
                desc_ep->bmAttributes.usage == 0x00)
              openEpIn(rhport, audio, itf, desc_ep, p_desc_for_params);

            if (isEpOutEnabled()) {
              if (tu_edpt_dir(ep_addr) == TUSB_DIR_OUT)
                openEpOut(rhport, audio, itf, desc_ep);
              if (isFeedbackEpEnabled() &&
                  tu_edpt_dir(ep_addr) == TUSB_DIR_IN &&
                  desc_ep->bmAttributes.usage == 1)
                openEpFeedback(audio, desc_ep);
            }
            foundEPs += 1;
          }
          p_desc = tu_desc_next(p_desc);
        }

        if (foundEPs != nEps) return true;  // ZLP already sent

        if (tud_audio_set_itf_cb_) tud_audio_set_itf_cb_(this, rhport, nullptr);

        setupFeedback(audio, func_id, alt);
        return true;
      }
      p_desc = tu_desc_next(p_desc);
    }
    return true;
  }

  // ── Main SET_INTERFACE handler ────────────────────────────────────────
  bool audiod_set_interface(uint8_t rhport,
                            tusb_control_request_t const* p_request) {
    uint8_t const itf = tu_u16_low(p_request->wIndex);
    uint8_t const alt = tu_u16_low(p_request->wValue);
    LOGD("SET_ITF itf=%u alt=%u [start]", itf, alt);

    uint8_t func_id, idxItf;
    uint8_t const* p_desc;
    if (!audiod_get_AS_interface_index_global(itf, &func_id, &idxItf,
                                              &p_desc)) {
      LOGD("  AS interface %u not found", itf);
      tud_control_status(rhport, p_request);
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

    tud_control_status(rhport, p_request);
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
      usbd_sof_enable(rhport, SOF_CONSUMER_AUDIO, enable_sof);
    }
    if (isEpInEnabled() && isEpInFlowControlEnabled())
      audiod_calc_tx_packet_sz(audio);

    return true;
  }

  bool audiod_set_fb_params_freq(audiod_function_t* audio, uint32_t sample_freq,
                                 uint32_t mclk_freq) {
    // Check if frame interval is within sane limits
    // The interval value n_frames was taken from the descriptors within

    // n_frames_min is ceil(2^10 * f_s / f_m) for full speed and ceil(2^13 * f_s
    // / f_m) for high speed this lower limit ensures the measures feedback
    // value has sufficient precision
    uint32_t const k = (TUSB_SPEED_FULL == tud_speed_get()) ? 10 : 13;
    uint32_t const n_frame = (1UL << audio->feedback.frame_shift);

    if ((((1UL << k) * sample_freq / mclk_freq) + 1) > n_frame) {
      LOGE("  UAC2 feedback interval too small");
      TU_BREAKPOINT();
      return false;
    }

    // Check if parameters really allow for a power of two division
    if ((mclk_freq % sample_freq) == 0 &&
        tu_is_power_of_two(mclk_freq / sample_freq)) {
      audio->feedback.compute_method =
          AUDIO_FEEDBACK_METHOD_FREQUENCY_POWER_OF_2;
      audio->feedback.compute.power_of_2 =
          (uint8_t)(16 - (audio->feedback.frame_shift - 1) -
                    tu_log2(mclk_freq / sample_freq));
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
    TU_VERIFY(audio->format_type_tx == AUDIO_FORMAT_TYPE_I);
    TU_VERIFY(audio->n_channels_tx);
    TU_VERIFY(audio->n_bytes_per_sample_tx);
    TU_VERIFY(audio->interval_tx);
    TU_VERIFY(audio->sample_rate_tx);

    // Restart the fractional accumulator for this streaming session.
    audio->tx_sample_acc = 0;

    const uint8_t interval = (tud_speed_get() == TUSB_SPEED_FULL)
                                 ? audio->interval_tx
                                 : 1 << (audio->interval_tx - 1);

    const uint16_t sample_normimal =
        (uint16_t)(audio->sample_rate_tx * interval /
                   ((tud_speed_get() == TUSB_SPEED_FULL) ? 1000 : 8000));
    const uint16_t sample_reminder =
        (uint16_t)(audio->sample_rate_tx * interval %
                   ((tud_speed_get() == TUSB_SPEED_FULL) ? 1000 : 8000));

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
    TU_ASSERT(packet_sz_tx_max <= audio->ep_in_sz);

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
    const uint32_t denom = (tud_speed_get() == TUSB_SPEED_FULL) ? 1000u : 8000u;
    const uint8_t iv = audio->interval_tx ? audio->interval_tx : 1;
    const uint32_t interval =
        (tud_speed_get() == TUSB_SPEED_FULL) ? iv : (1u << (iv - 1));

    audio->tx_sample_acc += audio->sample_rate_tx * interval;
    const uint32_t samples = audio->tx_sample_acc / denom;
    audio->tx_sample_acc -= samples * denom;

    uint32_t bytes =
        samples * audio->n_channels_tx * audio->n_bytes_per_sample_tx;
    if (bytes > audio->ep_in_sz) bytes = audio->ep_in_sz;
    return (uint16_t)bytes;
  }

  uint16_t tud_audio_n_write(uint8_t func_id, const void* data, uint16_t len) {
    TU_VERIFY(func_id < getAudioCount() && audiod_fct_[func_id].p_desc != NULL);
    // Always write to the FIFO. In linear-buffer mode the xfer_cb drains the
    // FIFO into lin_buf_in after DMA has finished, so there is no race.
    return tu_fifo_write_n(&audiod_fct_[func_id].ep_in_ff, data, len);
  }

  uint16_t tud_audio_n_available(uint8_t func_id) {
    TU_VERIFY(func_id < getAudioCount() && audiod_fct_[func_id].p_desc != NULL);
    return tu_fifo_count(&audiod_fct_[func_id].ep_out_ff);
  }

  uint16_t tud_audio_n_read(uint8_t func_id, void* buffer, uint16_t bufsize) {
    TU_VERIFY(func_id < getAudioCount() && audiod_fct_[func_id].p_desc != NULL);
    return tu_fifo_read_n(&audiod_fct_[func_id].ep_out_ff, buffer, bufsize);
  }
};

}  // namespace audio_tools

// Custom driver registration — routes to whichever USBAudioDeviceBase subclass
// was constructed (base or derived; set via s_active_ in the constructor).
extern "C" usbd_class_driver_t const* usbd_app_driver_get_cb(uint8_t* count) {
  return audio_tools::USBAudioDeviceBase::activeInstance().getClassDriver(
      count);
}
