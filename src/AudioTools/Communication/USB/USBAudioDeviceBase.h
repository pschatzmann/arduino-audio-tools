#pragma once

// Some cores (e.g. Arduino's Renesas UNO R4 core, Arduino.h) #define abs(x)
// as a function-like macro for AVR-era C compatibility. Since Arduino.h is
// always included ahead of library headers in a .ino, that macro is already
// active here and breaks libstdc++'s own templated abs(duration<...>)
// overload inside <chrono> (pulled in by <mutex>) -- the preprocessor sees
// the comma inside duration<_Rep, _Period> as a second macro argument.
// Suppress the macro for just this header's STL includes, then restore it
// so nothing else in the translation unit is affected.
#pragma push_macro("abs")
#undef abs

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <mutex>
#include <vector>

#pragma pop_macro("abs")

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
#if !defined(USE_TINYUSB) && !defined(ARDUINO_ARCH_STM32) && \
    !defined(ARDUINO_ARCH_RENESAS) && !defined(IS_ZEPHYR)
#error This Microcontroller has no Native USB interface
#endif
#endif

#include "AudioLogger.h"
#include "AudioTools/CoreAudio/BaseStream.h"
#include "AudioTools/Communication/USB/USBAudioBackend.h"
#include "AudioTools/Communication/USB/USBAudioConstants.h"
#include "AudioTools/Communication/USB/USBAudioStreamBase.h"
#include "USBAudio2DescriptorBuilder.h"

#define USB_DESCR_MAX_LEN 512

namespace audio_tools {


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
class USBAudioDeviceBase : public USBAudioStreamBase {
public:
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
    // ep_in/ep_out are written from the USB interrupt handler (openEpIn/
    // openEpOut/closeEpIn/closeEpOut, reached via audiod_set_interface() in
    // ISR context) and read from normal application code (write()/
    // readBytes()/isStreamingActiveTx()/isStreamingActiveRx(), called from
    // loop()). Without a memory-visibility guarantee across that boundary, a
    // compiler is free to cache a stale read indefinitely -- observed in
    // practice on the arm-none-eabi-gcc 7.2.1 toolchain used by the Arduino
    // Renesas UNO R4 core: isStreamingActiveTx() kept reporting false while
    // real ISO-IN transfers were actively completing, so write() silently
    // discarded all audio data (see the "disregard data" branch below) while
    // still reporting success.
    volatile uint8_t ep_in;  // TX audio data EP.
    uint16_t ep_in_sz;      // Current size of TX EP
    uint8_t
        ep_in_as_intf_num;  // Standard AS Interface Descriptor number for IN
    volatile uint8_t ep_out;  // RX audio data EP.
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

      core = getActualCore();
      LOGI("USB on core: %d", core);

      // Resize platform buffers — virtual so each platform uses the right API.
      resizeBuffers();

      const int n = getAudioCount();
      // GET_RANGE response is 2 + count*12 bytes; 64 is the floor needed for
      // everything else sharing this buffer (Feature Unit GET_RANGE, etc.).
      uint16_t cb_sz = 64u;
      if (config_.enable_multi_sample_rate) {
        uint16_t needed =
            (uint16_t)(2 + config_.supported_sample_rates.size() * 12);
        if (needed > cb_sz) cb_sz = needed;
      }
      ctrl_buf_sz_.assign(n, cb_sz);

      uint8_t desc[USB_DESCR_MAX_LEN];
      uint16_t desc_len = descr_builder.buildFullDescriptor(desc);
      desc_len_.assign(n, desc_len);

      // master (index 0) + one per channel
      setupVolumeState();


      audiod_init();

      if (!beginUSB()) {
        LOGE("beginUSB failed");
        return false;
      }
      is_started_ = true;
      rx_primed_ = false;  // re-arm the one-shot startup prime for this session
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


  /** @brief Returns true if the feedback endpoint is enabled: RX-only mode,
   *  or RXTX mode with enable_feedback_ep_rxtx.  When false the OUT endpoint
   *  (if any) is adaptive, ie: the host ignores the device's clock, and the
   *  setFeedback*() calls have no effect. */
  bool isFeedbackEpEnabled() const { return descr_builder.enableFeedbackEp(); }

  /** @brief Returns true if IN endpoint flow control is enabled. When on, the
   *  per-frame isochronous packet size is varied so non-integer sample-per-
   *  frame rates (e.g. 44100 Hz) are delivered at the exact average rate. */
  bool isEpInFlowControlEnabled() const {
    return config_.enable_ep_in_flow_control;
  }



  /** @brief Returns true if the host has opened the IN (capture) stream. */
  bool isStreamingActiveTx() const override {
    for (const auto& fct : audiod_fct_) {
      if (fct.ep_in != 0) return true;
    }
    return false;
  }

  /** @brief Returns true if the host has opened the OUT (playback) stream. */
  bool isStreamingActiveRx() const override {
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
  bool mounted() override { return backend().mounted(); }

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
      std::function<bool(USBAudioDeviceBase*, uint8_t, audiod_function_t*, uint16_t)>
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

  /**
   * @brief Overrides the automatic (FIFO_COUNT) feedback computation with a
   * fixed value driven by the caller, e.g. from an external buffer-fill
   * measurement.
   *
   * The USB audio feedback endpoint tells the host how fast to send/receive
   * samples so its clock stays in sync with the device's clock. Instead of
   * the built-in algorithm (which watches bufferRx()'s fill level), call
   * this repeatedly -- e.g. once per loop() -- with a value derived from
   * your own buffer: 0 asks the host to slow down (maps to the negotiated
   * min feedback value), 100 asks it to speed up (max value), and 50
   * approximates the nominal sample rate.
   *
   * Only has an effect when isFeedbackEpEnabled(); see
   * enable_feedback_ep_rxtx for RXTX mode.
   *
   * Each step is 1% of a +-1 sample per frame range (eg: ~420 ppm at 48 kHz),
   * much coarser than typical clock differences; see setFeedbackPpm() for
   * finer control.
   *
   * @param percent 0..100; values above 100 are clamped.
   */
  void setFeedbackPercent(int percent) {
    int normalized = percent;
    if (normalized < 0) normalized = 0;
    if (normalized > 100) normalized = 100;
    manual_feedback_ppm_ = INT32_MIN;
    manual_feedback_percent_ = normalized;
  }

  /**
   * @brief Like setFeedbackPercent(), but as an offset from the nominal
   * sample rate in parts per million: positive asks the host to send faster,
   * negative slower.  Resolution is the feedback format's (~0.3 ppm at
   * 48 kHz full speed), so a control loop on the application's own buffer
   * level can match the host's rate closely.  Clamped to the negotiated
   * +-1 sample per frame range.  Only has an effect when
   * isFeedbackEpEnabled().
   */
  void setFeedbackPpm(int32_t ppm) {
    manual_feedback_percent_ = -1;
    manual_feedback_ppm_ = ppm == INT32_MIN ? INT32_MIN + 1 : ppm;
  }

  /** @brief Reverts setFeedbackPercent()/setFeedbackPpm() and restores the
   *  automatic (FIFO_COUNT) feedback computation. */
  void setFeedbackAutomatic() {
    manual_feedback_percent_ = -1;
    manual_feedback_ppm_ = INT32_MIN;
  }


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
  /// Bytes silently discarded because bufferRx() was full when they arrived.
  /// Non-zero means audio is being lost -- increase fifo_packets or drain
  /// bufferRx() (readBytes()) faster/more regularly.
  uint32_t getRxDroppedBytes() const { return rx_dropped_bytes_; }
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

  virtual int getActualCore() const { return -1;}

 protected:
  bool tx_xfer_armed_ = false;
  volatile uint32_t xfer_cb_tx_count_ = 0;
  volatile uint32_t tx_fifo_read_total_ = 0;
  volatile uint32_t xfer_cb_rx_count_ = 0;
  volatile uint32_t rx_total_bytes_ = 0;
  volatile uint32_t rx_dropped_bytes_ = 0;
  volatile uint16_t tx_frame_bytes_last_ = 0;
  volatile uint32_t tx_xferred_last_ = 0;
  USBAudioConfig active_config_;
  USBAudio2DescriptorBuilder descr_builder{config_};
  std::function<void(USBAudioDeviceBase*, uint8_t rhport)> int_done_cb_;
  std::function<bool(USBAudioDeviceBase*, uint8_t rhport, audiod_function_t*, uint16_t bytes)>
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

  // Manual feedback override: -1 means "automatic" (use the negotiated
  // compute_method, e.g. AUDIO_FEEDBACK_METHOD_FIFO_COUNT); 0..100 means
  // the caller is driving the feedback value directly via
  // setFeedbackPercent(). See tud_audio_feedback_interval_isr().
  int16_t manual_feedback_percent_ = -1;
  // setFeedbackPpm() override, or INT32_MIN for none.
  volatile int32_t manual_feedback_ppm_ = INT32_MIN;
  // enable_ep_in_rate_tracking: frames to wait before the next correction.
  uint8_t tx_rate_holdoff_ = 0;

  // calculate!
  std::vector<uint16_t> desc_len_;
  // 64
  std::vector<uint16_t> ctrl_buf_sz_;

  std::vector<audiod_function_t> audiod_fct_;

  // s_active_ lets the class-driver trampolines and the static process()
  // trampoline reach the last-constructed instance without a singleton.
  inline static USBAudioDeviceBase* s_active_ = nullptr;


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
  void serviceUSB() override = 0;

  /** @brief Returns the backend used for all raw USB-stack calls (TinyUSB
   *  today, or a future native-HAL backend). Must be overridden by every
   *  platform subclass; the returned reference must outlive this object. */
  virtual USBAudioBackend& backend() = 0;


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

  /// Notify the host about a device-initiated volume change.
  void onVolumeChanged(uint8_t channel) override {
    sendInterruptNotification(AUDIO_FU_CTRL_VOLUME, channel,
                              USBAudio2DescriptorBuilder::ENTITY_FU1);
  }

  /// Notify the host about a device-initiated mute change.
  void onMuteChanged(uint8_t channel) override {
    sendInterruptNotification(AUDIO_FU_CTRL_MUTE, channel,
                              USBAudio2DescriptorBuilder::ENTITY_FU1);
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

  // Returns the control buffer size for a given function number
  uint16_t getCtrlBufSz(uint8_t fn) const {
    return (fn < ctrl_buf_sz_.size()) ? ctrl_buf_sz_[fn] : 64;
  }

  // Returns the descriptor length for a given function number
  uint16_t getDescLen(uint8_t fn) const {
    return (fn < desc_len_.size()) ? desc_len_[fn] : 0;
  }

  // Max Bytes for one 1 ms isochronous USB packet.
  uint16_t packetSize() const { return descr_builder.calcMaxPacketSize(); }

  // Returns the reset size for audiod_function_t up to and including
  // ctrl_buf_sz
  static constexpr size_t getResetSize() {
    return offsetof(audiod_function_t, ctrl_buf_sz) +
           sizeof(((audiod_function_t*)0)->ctrl_buf_sz);
  }

  // Called by audiod_sof_isr() at the feedback interval, in the USB
  // interrupt.  Only computes the feedback value: sending it means claiming
  // the endpoint, which takes TinyUSB's endpoint mutex, and doing that here
  // deadlocks if the interrupt arrived while the TinyUSB task held it.  The
  // value is sent from task context instead, each time the previous feedback
  // transfer completes (audiod_xfer_cb()), as TinyUSB's own audio driver does.
  void tud_audio_feedback_interval_isr(uint8_t func_id,
                                       uint32_t /*frame_count*/,
                                       uint8_t frame_shift) {
    audiod_function_t* audio = &audiod_fct_[func_id];

    int32_t ppm = manual_feedback_ppm_;
    if (ppm != INT32_MIN) {
      // setFeedbackPpm() override: offset from the nominal rate.
      uint32_t const frame_div = backend().isFullSpeed() ? 1000u : 8000u;
      int64_t nominal = ((int64_t)config_.sample_rate << 16) / frame_div;
      int64_t value = nominal + nominal * ppm / 1000000;
      if (value < (int64_t)audio->feedback.min_value)
        value = audio->feedback.min_value;
      if (value > (int64_t)audio->feedback.max_value)
        value = audio->feedback.max_value;
      audio->feedback.value = (uint32_t)value;
    } else if (manual_feedback_percent_ >= 0) {
      // setFeedbackPercent() override: scale linearly across the negotiated
      // [min_value, max_value] range instead of running the automatic
      // compute_method below.
      uint32_t range = audio->feedback.max_value - audio->feedback.min_value;
      audio->feedback.value =
          audio->feedback.min_value +
          (range * (uint32_t)manual_feedback_percent_) / 100;
    } else {
      switch (audio->feedback.compute_method) {
        case AUDIO_FEEDBACK_METHOD_FIFO_COUNT: {
          // In linear-buffer mode, audio data flows lin_buf_out → bufferRx(),
          // completely bypassing ep_out_ff.  Reading ep_out_ff would always
          // return 0, driving the feedback to min_value and causing the host
          // to gradually reduce its send rate until the buffer drains
          // (audible periodic drop every 5-10 s).  Use the platform RX ring
          // buffer level.
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
            // Buffer fuller than the target level: ask the host to slow
            // down. rate_const[1] is scaled so avg==fifo_depth (full) maps
            // exactly to min_value.
            uint32_t drop =
                (uint32_t)audio->feedback.compute.fifo_count.rate_const[1] *
                (avg - thr);
            audio->feedback.value =
                (nom > drop) ? nom - drop : audio->feedback.min_value;
          } else {
            // Buffer emptier than the target level: ask the host to speed
            // up. rate_const[0] is scaled so avg==0 (empty) maps exactly to
            // max_value.
            audio->feedback.value =
                nom +
                (uint32_t)audio->feedback.compute.fifo_count.rate_const[0] *
                    (thr - avg);
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
    }

  }

  // Task context only (see tud_audio_feedback_interval_isr()).
  void audiod_fb_kick(audiod_function_t* audio) {
    if (audio->ep_fb && backend().claimEndpoint(audio->rhport, audio->ep_fb))
      audiod_fb_send(audio);
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
        // Max packet across all advertised rates (see supported_sample_rates).
        uint16_t max_pkt = descr_builder.calcPacketSizeForRate(
            highestSupportedSampleRate(config_));
        audio->lin_buf_in.resize(max_pkt);
      }
      // Initialize OUT EP linear (DMA staging) buffer.
      if (isEpOutEnabled()) {
        uint16_t max_pkt = descr_builder.calcPacketSizeForRate(
            highestSupportedSampleRate(config_));
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

        uint16_t frame_bytes = isEpInFlowControlEnabled()
                                   ? audiod_tx_packet_size_fc(audio)
                                   : audio->ep_in_sz;
        if (frame_bytes > audio->ep_in_sz) frame_bytes = audio->ep_in_sz;
        if (tx_done_cb_) tx_done_cb_(this, rhport, audio, frame_bytes);

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
        if (xferred_bytes > 0) {
          int written = bufferRx().writeArray(audio->lin_buf_out.data(),
                                              (int)xferred_bytes);
          if ((uint32_t)written < xferred_bytes)
            rx_dropped_bytes_ += xferred_bytes - (uint32_t)written;
        }
        // Re-arm the OUT endpoint before invoking the callback: rx_done_cb_
        // is user code (e.g. it may push the packet through a resampler)
        // and its duration is out of our control. If the next transfer
        // isn't queued until after it returns, a slow callback delays
        // re-arming past the next isochronous OUT window and the host's
        // packet for that frame is lost - audible as clicks/dropouts,
        // and more likely to happen the larger each packet is (e.g. at
        // 44.1/48kHz vs. 32kHz).
        (void)backend().transfer(rhport, audio->ep_out,
                                 audio->lin_buf_out.data(), audio->ep_out_sz);
        if (rx_done_cb_)
          rx_done_cb_(this, rhport, audio, (uint16_t)xferred_bytes);
        return true;
      }
      if (isFeedbackEpEnabled() && audio->ep_fb == ep_addr) {
        if (fb_done_cb_) fb_done_cb_(this, func_id);
        // Queue the next feedback packet with the latest value.
        audiod_fb_kick(audio);
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
          const auto& rates = config_.supported_sample_rates;
          uint16_t cnt = (uint16_t)rates.size();
          memcpy(cb, &cnt, 2);
          for (size_t i = 0; i < rates.size(); i++) {
            uint32_t r = rates[i];
            uint32_t z = 0;
            memcpy(cb + 2 + i * 12,     &r, 4);
            memcpy(cb + 2 + i * 12 + 4, &r, 4);
            memcpy(cb + 2 + i * 12 + 8, &z, 4);
          }
          return backend().controlTransfer(
              rhport, p_request, cb,
              (uint16_t)(2 + rates.size() * 12));
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
        // Start the feedback transfers; each completion queues the next.
        if (isFeedbackEpEnabled() && audio->ep_fb) {
          if (audio->feedback.value == 0)
            audio->feedback.value = audio->feedback.min_value / 2 + audio->feedback.max_value / 2;
          audiod_fb_kick(audio);
        }
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
    tx_rate_holdoff_ = 0;

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
    uint32_t samples = audio->tx_sample_acc / denom;
    audio->tx_sample_acc -= samples * denom;

    const uint32_t frame_bytes =
        audio->n_channels_tx * audio->n_bytes_per_sample_tx;
    if (config_.enable_ep_in_rate_tracking) {
      // Asynchronous IN: the application writes bufferTx() at the device's
      // clock, which differs slightly from the host's.  Follow it by sending
      // one sample more when the buffer is above half full, one less when
      // below.  The hold-off limits how quickly corrections come (and so
      // how much the packet sizes jitter): at most 1 sample per 8 packets,
      // ie: 2600 ppm at 48 kHz, far more than any crystal's error.
      if (tx_rate_holdoff_) {
        tx_rate_holdoff_--;
      } else {
        const uint32_t level = (uint32_t)bufferTx().available() / frame_bytes;
        const uint32_t target = (uint32_t)bufferTx().size() / frame_bytes / 2;
        if (level > target + 1) {
          samples++;
          tx_rate_holdoff_ = 7;
        } else if (level + 1 < target && samples > 1) {
          samples--;
          tx_rate_holdoff_ = 7;
        }
      }
    }

    uint32_t bytes = samples * frame_bytes;
    if (bytes > audio->ep_in_sz) bytes = audio->ep_in_sz;
    return (uint16_t)bytes;
  }

};

}  // namespace audio_tools
