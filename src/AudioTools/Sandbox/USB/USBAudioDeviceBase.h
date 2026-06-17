#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <mutex>
#include <vector>

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

#include "USBAudio2DescriptorBuilder.h"
#include "USBAudioConfig.h"

extern "C" {
#include "device/usbd.h"
#include "device/usbd_pvt.h"
#include "tusb.h"
}

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
#ifndef AUDIO_CS_AC_INTERFACE_INPUT_TERMINAL
#define AUDIO_CS_AC_INTERFACE_INPUT_TERMINAL \
  AUDIO10_CS_AC_INTERFACE_INPUT_TERMINAL
#endif
#ifndef AUDIO_CS_CTRL_CLK_VALID
#define AUDIO_CS_CTRL_CLK_VALID 0x02u
#endif
#ifndef AUDIO_CS_REQ_RANGE
#define AUDIO_CS_REQ_RANGE 0x02u
#endif
#endif

namespace audio_tools {

/**
 * @brief USB Audio Device class for audio streaming over USB.
 *
 * This class implements a USB audio device, providing configuration, descriptor
 * management, endpoint setup, and control request handling for audio streaming
 * over USB. It supports multiple audio formats, feedback methods, and endpoint
 * configurations, and is designed for use with TinyUSB or native USB on
 * supported MCUs.
 */
class USBAudioDeviceBase {
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
  USBAudioDeviceBase() { s_active_ = this; }

  USBAudioDeviceBase(USBAudioConfig cfg) : USBAudioDeviceBase() {
    config_ = cfg;
  }

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
#ifdef ESP32
    // ESP32's USB DCD does not support DMA-direct-to-tu_fifo_t for isochronous
    // OUT endpoints (usbd_edpt_xfer_fifo fails silently), so ep_out_ff is never
    // written and no audio data arrives.  Linear-buffer mode uses a plain
    // uint8_t* DMA target which all DCDs support; audiod_xfer_cb then copies
    // each received packet into ep_out_ff via tu_fifo_write_n.
    if (cfg.enable_ep_out) cfg.use_linear_buffer_rx = true;
#endif
    return cfg;
  }

  virtual size_t getTotalBytesRx() const { return total_rx_bytes_; }

  virtual size_t getTotalBytesTx() const { return total_tx_bytes_; }

  bool begin(const USBAudioConfig& cfg) {
    if (is_started_ && !configChanged(cfg))
      return true;  // active, unchanged: no-op
    config_ = cfg;
    return begin();
  }

  bool begin() {
    total_rx_bytes_ = 0;
    total_tx_bytes_ = 0;
    const int n = getAudioCount();
    ctrl_buf_sz_.assign(n,
                        64u);  // 64 bytes is the USB control transfer standard

    const uint16_t sw_buf = fifoSize();
    ep_in_sw_buf_sz_.assign(n, sw_buf);
    ep_out_sw_buf_sz_.assign(n, sw_buf);

    uint16_t desc_len = 0;
    descr_builder.buildFullDescriptor(&desc_len);
    desc_len_.assign(n, desc_len);

    const uint16_t pkt = packetSize();
    process_buf_tx_.assign(pkt, 0);
    process_buf_rx_.assign(pkt, 0);

    audiod_init();
    is_started_ = true;
    return beginUSB();
  }
  /**
   * @brief Set the USB audio configuration.
   * @param cfg Reference to a USBAudioConfig structure with desired settings.
   */
  void setConfig(const USBAudioConfig& cfg) { config_ = cfg; }

  /**
   * @brief Returns the most-recently-constructed instance (base or subclass).
   *
   * Set in the constructor via s_active_, so usbd_app_driver_get_cb() and the
   * static process() trampoline can reach the right object without a singleton.
   */
  static USBAudioDeviceBase& activeInstance() { return *s_active_; }

  /** @brief Override in platform subclasses to force USB re-enumeration. */
  virtual void reenumerateUSB() {}

  /**
   * @brief Apply a new config and re-enumerate only if it differs from the
   *        current one.  String fields (manufacturer, product, serial) are
   *        compared by content (strcmp), not pointer value.
   */
  void reenumerateUSBOnChange(const USBAudioConfig& cfg) {
    if (configChanged(cfg)) {
      setConfig(cfg);
      begin();  // rebuilds descriptor and re-enumerates via beginUSB()
    }
  }

  /** @brief Override in platform subclasses to register descriptors and start
   *         the USB host-controller stack (e.g. USB.begin() on ESP32).
   *         Called at the end of begin(cfg, info).  The base no-op is correct
   *         for RP2040 where TinyUSB is started by the system before setup().*/
  virtual bool beginUSB() { return true; }

  /** @brief Returns true if the IN endpoint is enabled. */
  bool getEnableEpIn() const { return config_.enable_ep_in; }
  /** @brief Returns true if the OUT endpoint is enabled. */
  bool getEnableEpOut() const { return config_.enable_ep_out; }
  /** @brief Returns true if the feedback endpoint is enabled.
   *  Only meaningful in pure RX (OUT-only) mode: with an IN endpoint present
   *  the host uses the TX stream as implicit feedback instead. */
  bool getEnableFeedbackEp() const { return descr_builder.enableFeedbackEp(); }
  /** @brief Returns true if IN endpoint flow control is enabled. */
  bool getEnableEpInFlowControl() const { return false; }

  /** @brief Returns true when the host has selected the streaming alternate
   *  setting (alt > 0) and opened the isochronous IN endpoint.  Use this to
   *  decide whether to buffer TX audio or silently discard it. */
  bool streamingActive() const {
    for (const auto& fct : audiod_fct_) {
      if (fct.ep_in != 0) return true;
    }
    return false;
  }
  /** @brief Returns true if the interrupt endpoint is enabled. */
  bool getEnableInterruptEp() const { return false; }
  /** @brief Returns true if FIFO mutex is enabled. */
  bool getEnableFifoMutex() const { return true; }

  /** @brief Returns the number of audio functions (always 1). */
  uint8_t getAudioCount() const { return 1; }

  /**
   * @brief Get the USB audio descriptors for the specified interface and
   * alternate setting.
   * @param itf Interface number.
   * @param alt Alternate setting number.
   * @param out_length Pointer to store the length of the descriptor.
   * @return Pointer to the descriptor data.
   */
  const uint8_t* getAudioDescriptors(uint8_t /*itf*/, uint8_t /*alt*/,
                                     uint16_t* out_length) {
    return descr_builder.buildFullDescriptor(out_length);
  }

  /** @brief Returns true if the device is mounted by the USB host. */
  bool mounted() const { return tud_mounted(); }

  /**
   * @brief Handle a USB control request for the audio device.
   * @param request Pointer to the control request structure.
   * @param buffer Data buffer for the request.
   * @param length Length of the buffer.
   * @return true if the request was handled, false otherwise.
   */
  bool handleControlRequest(const tusb_control_request_t* request, void* buffer,
                            uint16_t length) {
    // Example: handle standard GET_DESCRIPTOR request for audio interface
    if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD &&
        request->bRequest == TUSB_REQ_GET_DESCRIPTOR) {
      uint16_t desc_len;

      uint8_t const itf = tu_u16_low(request->wIndex);
      uint8_t const alt = tu_u16_low(request->wValue);

      const uint8_t* desc = getAudioDescriptors(itf, alt, &desc_len);
      if (desc && buffer && length >= desc_len) {
        std::memcpy(buffer, desc, desc_len);
        return true;
      }
    }
    // TODO: handle class-specific requests (mute, volume, etc.)
    return false;
  }

  /**
   * @brief Main processing loop for USB audio streaming.
   *
   * Call this method regularly in your main loop or USB task to handle
   * audio data transmission and reception.
   */
  void process() {
#ifdef USE_TINYUSB
    // RP2040: the app drives the USB stack; this also triggers audiod_xfer_cb
    // which is the sole TX drainer (fills ep_in_ff at the correct 1-ms rate).
    tud_task();
#endif
    // ESP32: the USB FreeRTOS task drives tud_task() and audiod_xfer_cb — no
    // need to drain TX here; doing so would over-drain and send silence.
    if (process_buf_rx_.empty()) return;
    const uint16_t pkt = (uint16_t)process_buf_rx_.size();
    if (config_.enable_ep_out && tud_ready()) {
      // Snapshot bytes available now — don't chase new arrivals from the USB
      // ISR (one packet/ms).  Without this the drain loop never exits,
      // freezing loop() while audio is streaming.
      uint16_t avail = available();
      uint16_t n = 0;
      while (avail > 0 && (n = read(process_buf_rx_.data(), pkt)) > 0) {
        if (rx_callback_) rx_callback_(process_buf_rx_.data(), n);
        total_rx_bytes_ += n;
        avail = (avail >= n) ? avail - n : 0;
      }
      yield();
    }
  }

  uint16_t available() {
    for (uint8_t i = 0; i < (uint8_t)audiod_fct_.size(); i++) {
      if (audiod_fct_[i].ep_out == config_.ep_out) {
        return tud_audio_n_available(i);
      }
    }
    return 0;
  }

  /**
   * @brief Register a callback for received audio data (OUT endpoint).
   * @param cb Callback function with (const uint8_t* data, uint16_t length).
   */
  void setRxCallback(std::function<void(const uint8_t*, uint16_t)> cb) {
    rx_callback_ = cb;
  }

  /**
   * @brief Register a callback for transmitting audio data (IN endpoint).
   * @param cb Callback function with (const uint8_t* data, uint16_t length)
   * returning bytes written.
   */
  void setTxCallback(std::function<uint16_t(uint8_t*, uint16_t)> cb) {
    tx_callback_ = cb;
  }

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
   * @brief Register a callback for GET requests on an entity.
   * @param cb Callback function.
   */
  void setGetReqEntityCallback(
      std::function<bool(USBAudioDeviceBase*, uint8_t,
                         tusb_control_request_t const*)>
          cb) {
    get_req_entity_cb_ = cb;
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
   * @brief Register a callback for audio TX done events.
   * @param cb Callback function.
   */
  void setAudiodTxDoneCallback(
      std::function<bool(USBAudioDeviceBase*, uint8_t, audiod_function_t*)>
          cb) {
    audiod_tx_done_cb_ = cb;
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

  /** @brief Send audio data to the host (device → host, microphone/capture). */
  uint16_t writeAudio(const void* data, uint16_t len) {
    return write(data, len);
  }

  /** @brief Receive audio data from the host (host → device, speaker/playback).
   */
  uint16_t readAudio(void* buffer, uint16_t bufsize) {
    return read(buffer, bufsize);
  }

  /** @brief Bytes of received audio waiting in the RX FIFO. */
  uint16_t rxAvailable() {
    for (uint8_t i = 0; i < (uint8_t)audiod_fct_.size(); i++) {
      if (audiod_fct_[i].ep_out == config_.ep_out)
        return tu_fifo_count(&audiod_fct_[i].ep_out_ff);
    }
    return 0;
  }

  /** @brief Bytes of free space remaining in the TX FIFO (or linear TX buffer).
   */
  uint16_t txAvailableForWrite() {
    for (uint8_t i = 0; i < (uint8_t)audiod_fct_.size(); i++) {
      if (audiod_fct_[i].ep_in == config_.ep_in) {
        if (getUseLinearBufferTx())
          return (uint16_t)audiod_fct_[i].lin_buf_in.size();
        return tu_fifo_remaining(&audiod_fct_[i].ep_in_ff);
      }
    }
    return 0;
  }

  /** @brief Stop audio streaming and clear FIFOs. Does not disconnect USB. */
  void end() {
    // Null callbacks first so no stale closure can fire between clearing the
    // FIFOs and the caller registering new ones on the next begin().
    rx_callback_ = nullptr;
    tx_callback_ = nullptr;
    for (auto& audio : audiod_fct_) {
      tu_fifo_clear(&audio.ep_in_ff);
      tu_fifo_clear(&audio.ep_out_ff);
      std::fill(audio.lin_buf_in.begin(), audio.lin_buf_in.end(), 0);
      std::fill(audio.lin_buf_out.begin(), audio.lin_buf_out.end(), 0);
    }
    process_buf_tx_.clear();
    process_buf_rx_.clear();
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
  const uint8_t* getDescriptor(uint16_t* len) {
    return descr_builder.buildFullDescriptor(len);
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
  usbd_class_driver_t const* usbd_app_driver_get(uint8_t* count) {
    static usbd_class_driver_t driver;
    driver.name = "Audio";
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

 protected:
  static bool strSame(const char* a, const char* b) {
    if (a == b) return true;
    if (!a || !b) return false;
    return std::strcmp(a, b) == 0;
  }

  bool configChanged(const USBAudioConfig& n) const {
    const USBAudioConfig& o = config_;
    return o.sample_rate != n.sample_rate || o.channels != n.channels ||
           o.bits_per_sample != n.bits_per_sample ||
           o.enable_ep_in != n.enable_ep_in ||
           o.enable_ep_out != n.enable_ep_out ||
           o.enable_feedback_ep != n.enable_feedback_ep || o.ep_in != n.ep_in ||
           o.ep_out != n.ep_out || o.ep_fb != n.ep_fb ||
           o.itf_num_ac != n.itf_num_ac || o.vid != n.vid || o.pid != n.pid ||
           o.self_powered != n.self_powered ||
           o.max_power_ma != n.max_power_ma ||
           !strSame(o.manufacturer, n.manufacturer) ||
           !strSame(o.product, n.product) || !strSame(o.serial, n.serial);
  }

  bool is_started_ = false;
  size_t total_rx_bytes_ = 0;
  size_t total_tx_bytes_ = 0;
  USBAudioConfig config_;
  USBAudio2DescriptorBuilder descr_builder{config_};
  std::function<void(const uint8_t*, uint16_t)> rx_callback_;
  std::function<uint16_t(uint8_t*, uint16_t)> tx_callback_;
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
  // Callback for entity GET requests
  std::function<bool(USBAudioDeviceBase*, uint8_t rhport,
                     tusb_control_request_t const*)>
      get_req_entity_cb_;
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

  std::function<bool(USBAudioDeviceBase*, uint8_t rhport,
                     audiod_function_t* audio)>
      audiod_tx_done_cb_;

  std::function<void(USBAudioDeviceBase*, uint8_t func_id, uint8_t alt_itf,
                     audio_feedback_params_t* feedback_param)>
      tud_audio_feedback_params_cb_;

  std::function<bool(USBAudioDeviceBase*, uint8_t func_id)>
      tud_audio_feedback_format_correction_cb_;

  // TU_MAX(CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_OUT,
  // CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_OUT)
  std::vector<uint16_t> ep_out_sw_buf_sz_;
  //  (TUD_OPT_HIGH_SPEED ? 32 : 4) * CFG_TUD_AUDIO_EP_SZ_IN // Example write
  //  FIFO every 1ms, so it should be 8 times larger for HS device
  std::vector<uint16_t> ep_in_sw_buf_sz_;
  // calculate!
  std::vector<uint16_t> desc_len_;
  // 64
  std::vector<uint16_t> ctrl_buf_sz_;

  std::vector<audiod_function_t> audiod_fct_;
  std::vector<osal_mutex_def_t> ep_in_ff_mutex_wr_;
  std::vector<osal_mutex_def_t> ep_out_ff_mutex_rd_;
  std::vector<uint8_t> process_buf_tx_;
  std::vector<uint8_t> process_buf_rx_;

  // s_active_ lets usbd_app_driver_get_cb() and the static process() trampoline
  // reach the last-constructed instance without a singleton.
  inline static USBAudioDeviceBase* s_active_ = nullptr;

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

  bool getUseLinearBufferRx() const { return config_.use_linear_buffer_rx; }
  bool getUseLinearBufferTx() const { return config_.use_linear_buffer_tx; }

  // Bytes for one 1 ms isochronous USB packet.
  uint16_t packetSize() const { return descr_builder.calcMaxPacketSize(); }
  // Total audio FIFO size in bytes.
  uint16_t fifoSize() const {
    return (uint16_t)(packetSize() * config_.fifo_packets);
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

  // Write audio data to IN endpoint
  uint16_t write(const void* data, uint16_t len) {
    for (uint8_t i = 0; i < (uint8_t)audiod_fct_.size(); i++) {
      if (audiod_fct_[i].ep_in == config_.ep_in)
        return tud_audio_n_write(i, data, len);
    }
    return 0;
  }

  // Read audio data from OUT endpoint
  uint16_t read(void* buffer, uint16_t bufsize) {
    for (uint8_t i = 0; i < (uint8_t)audiod_fct_.size(); i++) {
      if (audiod_fct_[i].ep_out == config_.ep_out)
        return tud_audio_n_read(i, buffer, bufsize);
    }
    return 0;
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
      // Initialize IN EP FIFO if required
      if (getEnableEpIn()) {
        audio->ep_in_sw_buf.resize(getEpInSwBufSz(i));

        TUSB_FIFO_CONFIG(&audio->ep_in_ff, audio->ep_in_sw_buf.data(),
                         getEpInSwBufSz(i), true);
        if (getEnableFifoMutex()) {
          tu_fifo_config_mutex(&audio->ep_in_ff,
                               osal_mutex_create(&ep_in_ff_mutex_wr_[i]), NULL);
        }
      }
      if (getUseLinearBufferTx()) {
        audio->lin_buf_in.resize(fifoSize());
      }
      // Initialize OUT EP FIFO if required
      if (getEnableEpOut()) {
        audio->ep_out_sw_buf.resize(getEpOutSwBufSz(i));
        TUSB_FIFO_CONFIG(&audio->ep_out_ff, audio->ep_out_sw_buf.data(),
                         getEpOutSwBufSz(i), true);
        if (getEnableFifoMutex()) {
          tu_fifo_config_mutex(&audio->ep_out_ff, NULL,
                               osal_mutex_create(&ep_out_ff_mutex_rd_[i]));
        }
      }
      if (getUseLinearBufferRx()) {
        audio->lin_buf_out.resize(fifoSize());
      }
      if (getEnableFeedbackEp()) {
        audio->fb_buf.resize(1);  // one uint32_t = 4 bytes of feedback data
      }
    }
  }

  void alloc_mutex() {
    if (getEnableFifoMutex()) {
      if (getEnableEpIn()) {
        ep_in_ff_mutex_wr_.resize(getAudioCount());
      }
      if (getEnableEpOut()) {
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
      if (getEnableEpIn()) {
        tu_fifo_clear(&audio->ep_in_ff);
      }
      if (getEnableEpOut()) {
        tu_fifo_clear(&audio->ep_out_ff);
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
      TU_ASSERT(getEnableInterruptEp());
    }
    TU_VERIFY(itf_desc->bAlternateSetting == 0);
    uint8_t i;
    for (i = 0; i < getAudioCount(); i++) {
      if (!audiod_fct_[i].p_desc) {
        audiod_fct_[i].p_desc = (uint8_t const*)itf_desc;
        audiod_fct_[i].rhport = rhport;
        audiod_fct_[i].desc_length = getDescLen(i);
        if (getEnableEpIn() || getEnableEpOut() || getEnableFeedbackEp()) {
          uint8_t ep_in = 0, ep_out = 0, ep_fb = 0;
          uint16_t ep_in_size = 0, ep_out_size = 0;
          uint8_t const* p_desc = audiod_fct_[i].p_desc;
          uint8_t const* p_desc_end =
              p_desc + audiod_fct_[i].desc_length - TUD_AUDIO_DESC_IAD_LEN;
          while (p_desc_end - p_desc > 0) {
            if (tu_desc_type(p_desc) == TUSB_DESC_ENDPOINT) {
              tusb_desc_endpoint_t const* desc_ep =
                  (tusb_desc_endpoint_t const*)p_desc;
              if (desc_ep->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS) {
                if (getEnableFeedbackEp() && desc_ep->bmAttributes.usage == 1) {
                  ep_fb = desc_ep->bEndpointAddress;
                }
                if (desc_ep->bmAttributes.usage == 0) {
                  if (getEnableEpIn() &&
                      tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN) {
                    ep_in = desc_ep->bEndpointAddress;
                    ep_in_size =
                        TU_MAX(tu_edpt_packet_size(desc_ep), ep_in_size);
                  } else if (getEnableEpOut() &&
                             tu_edpt_dir(desc_ep->bEndpointAddress) ==
                                 TUSB_DIR_OUT) {
                    ep_out = desc_ep->bEndpointAddress;
                    ep_out_size =
                        TU_MAX(tu_edpt_packet_size(desc_ep), ep_out_size);
                  }
                }
              }
            }
            p_desc = tu_desc_next(p_desc);
          }
          if (getEnableEpIn() && ep_in) {
            usbd_edpt_iso_alloc(rhport, ep_in, ep_in_size);
          }
          if (getEnableEpOut() && ep_out) {
            usbd_edpt_iso_alloc(rhport, ep_out, ep_out_size);
          }
          if (getEnableFeedbackEp() && ep_fb) {
            usbd_edpt_iso_alloc(rhport, ep_fb, 4);
          }
        }
        // Scan for bclock_id_tx (clock entity referenced by the USB-streaming
        // terminal) and interval_tx.  Runs in TX, RX, and RXTX mode so that
        // clock-validity/frequency GET requests always succeed.
        //   TX/RXTX: Output Terminal type=USB_STREAMING → bCSourceID at [8]
        //   RX:      Input  Terminal type=USB_STREAMING → bCSourceID at [7]
        // interval_tx is only meaningful for the ISO IN endpoint (TX/RXTX).
        if (getEnableEpIn() || getEnableEpOut()) {
          uint8_t const* p_desc = audiod_fct_[i].p_desc;
          uint8_t const* p_desc_end =
              p_desc + audiod_fct_[i].desc_length - TUD_AUDIO_DESC_IAD_LEN;
          while (p_desc_end - p_desc > 0) {
            if (tu_desc_type(p_desc) == TUSB_DESC_ENDPOINT) {
              if (getEnableEpIn()) {
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
        if (getEnableInterruptEp()) {
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
                audiod_fct_[i].ep_int = ep_addr;
                TU_ASSERT(usbd_edpt_open(audiod_fct_[i].rhport, desc_ep));
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
            // Check if entity is present and get corresponding driver index
            TU_VERIFY(audiod_verify_entity_exists(itf, entityID, &func_id));

            if (getEnableEpIn() && getEnableEpInFlowControl()) {
              uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
              if (audiod_fct_[func_id].bclock_id_tx == entityID &&
                  ctrlSel == AUDIO_CS_CTRL_SAM_FREQ &&
                  p_request->bRequest == AUDIO_CS_REQ_CUR) {
                audiod_fct_[func_id].sample_rate_tx =
                    tu_unaligned_read32(audiod_fct_[func_id].ctrl_buf.data());
              }
            }

            // Invoke callback
            if (tud_audio_set_req_entity_cb_) {
              return tud_audio_set_req_entity_cb_(
                  this, rhport, p_request,
                  audiod_fct_[func_id].ctrl_buf.data());
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

  bool audiod_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result,
                      uint32_t xferred_bytes) {
    (void)result;
    (void)xferred_bytes;
    for (uint8_t func_id = 0; func_id < getAudioCount(); func_id++) {
      audiod_function_t* audio = &audiod_fct_[func_id];
      if (getEnableInterruptEp() && audio->ep_int == ep_addr) {
        if (int_done_cb_) int_done_cb_(this, rhport);
        return true;
      }
      if (getEnableEpIn() && audio->ep_in == ep_addr &&
          audio->alt_setting.size() != 0) {
        if (tx_done_cb_) tx_done_cb_(this, rhport, audio);
        // Refill ep_in_ff from the TX callback here (in TinyUSB's own task/ISR)
        // so the pipeline never stalls even if process() is blocked in loop().
        if (tx_callback_ && !process_buf_tx_.empty()) {
          const uint16_t pkt = (uint16_t)process_buf_tx_.size();
          std::fill(process_buf_tx_.begin(), process_buf_tx_.end(), 0);
          uint16_t n = tx_callback_(process_buf_tx_.data(), pkt);
          total_tx_bytes_ += n;
          if (n > 0) write(process_buf_tx_.data(), n);
        }
        if (getUseLinearBufferTx()) {
          // Drain FIFO into the DMA staging buffer, then re-arm.
          // lin_buf_in is safe to write now that the previous DMA has finished.
          uint16_t n = tu_fifo_read_n(
              &audio->ep_in_ff, audio->lin_buf_in.data(), audio->ep_in_sz);
          if (n == 0)
            std::fill(audio->lin_buf_in.begin(), audio->lin_buf_in.end(), 0);
          TU_VERIFY(TUSB_EDPT_XFER(rhport, audio->ep_in,
                                   audio->lin_buf_in.data(), audio->ep_in_sz));
        } else {
          TU_VERIFY(TUSB_EDPT_XFER_FIFO(rhport, audio->ep_in, &audio->ep_in_ff,
                                        audio->ep_in_sz));
        }
        return true;
      }
      if (getEnableEpOut() && audio->ep_out == ep_addr) {
        if (getUseLinearBufferRx()) {
          // Copy the DMA-received packet into the FIFO so tud_audio_n_read()
          // sees it, then re-arm the DMA into lin_buf_out.
          tu_fifo_write_n(&audio->ep_out_ff, audio->lin_buf_out.data(),
                          (uint16_t)xferred_bytes);
          if (rx_done_cb_)
            rx_done_cb_(this, rhport, audio, (uint16_t)xferred_bytes);
          // Must succeed: plain uint8_t* DMA target is supported on all DCDs.
          // If it fails the endpoint stalls; treat as non-fatal so the
          // xfer_cb at least returns true and doesn't confuse the USB stack.
          (void)TUSB_EDPT_XFER(rhport, audio->ep_out, audio->lin_buf_out.data(),
                               audio->ep_out_sz);
        } else {
          if (rx_done_cb_)
            rx_done_cb_(this, rhport, audio, (uint16_t)xferred_bytes);
          (void)TUSB_EDPT_XFER_FIFO(rhport, audio->ep_out, &audio->ep_out_ff,
                                    audio->ep_out_sz);
        }
        return true;
      }
      if (getEnableFeedbackEp() && audio->ep_fb == ep_addr) {
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
    if (getEnableEpOut() && getEnableFeedbackEp()) {
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

  bool audiod_control_request(uint8_t rhport,
                              tusb_control_request_t const* p_request) {
    (void)rhport;

    // Handle standard requests - standard set requests usually have no data
    // stage so we also handle set requests here
    if (p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD) {
      switch (p_request->bRequest) {
        case TUSB_REQ_GET_INTERFACE:
          return audiod_get_interface(rhport, p_request);

        case TUSB_REQ_SET_INTERFACE:
          return audiod_set_interface(rhport, p_request);

        case TUSB_REQ_CLEAR_FEATURE:
          return true;

        // Unknown/Unsupported request
        default:
          TU_BREAKPOINT();
          return false;
      }
    }

    // Handle class requests
    if (p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS) {
      uint8_t itf = TU_U16_LOW(p_request->wIndex);
      uint8_t func_id;

      // Conduct checks which depend on the recipient
      switch (p_request->bmRequestType_bit.recipient) {
        case TUSB_REQ_RCPT_INTERFACE: {
          uint8_t entityID = TU_U16_HIGH(p_request->wIndex);

          // Verify if entity is present
          if (entityID != 0) {
            // Find index of audio driver structure and verify entity really
            // exists
            TU_VERIFY(audiod_verify_entity_exists(itf, entityID, &func_id));

            // In case we got a get request invoke callback - callback needs to
            // answer as defined in UAC2 specification page 89 - 5. Requests
            if (p_request->bmRequestType_bit.direction == TUSB_DIR_IN) {
              // Handle clock source GET requests directly so the host can
              // validate the clock without requiring an application callback.
              // IMPORTANT: always use ctrl_buf (persistent) — never stack
              // buffers, because tud_control_xfer() starts a DMA transfer that
              // completes asynchronously after this function returns.
              if (entityID == audiod_fct_[func_id].bclock_id_tx) {
                uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
                uint8_t* cb = audiod_fct_[func_id].ctrl_buf.data();
                if (ctrlSel == AUDIO_CS_CTRL_CLK_VALID &&
                    p_request->bRequest == AUDIO_CS_REQ_CUR) {
                  cb[0] = 1;  // clock is always valid
                  return tud_control_xfer(rhport, p_request, cb, 1);
                }
                if (ctrlSel == AUDIO_CS_CTRL_SAM_FREQ) {
                  uint32_t rate = audiod_fct_[func_id].sample_rate_tx
                                      ? audiod_fct_[func_id].sample_rate_tx
                                      : (uint32_t)config_.sample_rate;
                  if (p_request->bRequest == AUDIO_CS_REQ_CUR) {
                    memcpy(cb, &rate, 4);
                    return tud_control_xfer(rhport, p_request, cb, 4);
                  }
                  if (p_request->bRequest == AUDIO_CS_REQ_RANGE) {
                    // UAC2 range: wNumSubRanges(2) + dMIN(4) + dMAX(4) +
                    // dRES(4)
                    memset(cb, 0, 14);
                    cb[0] = 1;                 // wNumSubRanges = 1 (LE)
                    memcpy(cb + 2, &rate, 4);  // dMIN
                    memcpy(cb + 6, &rate, 4);  // dMAX
                    // cb[10..13] = dRES = 0 (fixed clock)
                    return tud_control_xfer(rhport, p_request, cb, 14);
                  }
                }
              }
              if (req_entity_cb_) return req_entity_cb_(this, func_id);
              // Non-clock, no callback: fall through to send ctrl_buf (e.g.
              // volume GET).
            }
          } else {
            // Find index of audio driver structure and verify interface really
            // exists
            TU_VERIFY(audiod_verify_itf_exists(itf, &func_id));

            // In case we got a get request invoke callback - callback needs to
            // answer as defined in UAC2 specification page 89 - 5. Requests
            if (p_request->bmRequestType_bit.direction == TUSB_DIR_IN) {
              // Use callback if set, otherwise return false
              if (get_req_itf_cb_) {
                return get_req_itf_cb_(this, rhport, p_request);
              }
              return false;
            }
          }
        } break;

        case TUSB_REQ_RCPT_ENDPOINT: {
          uint8_t ep = TU_U16_LOW(p_request->wIndex);

          // Find index of audio driver structure and verify EP really exists
          TU_VERIFY(audiod_verify_ep_exists(ep, &func_id));

          // In case we got a get request invoke callback - callback needs to
          // answer as defined in UAC2 specification page 89 - 5. Requests
          if (p_request->bmRequestType_bit.direction == TUSB_DIR_IN) {
            // Use callback if set, otherwise return false
            if (get_req_ep_cb_) {
              return get_req_ep_cb_(this, rhport, p_request);
            }
            return false;
          }
        } break;

        // Unknown/Unsupported recipient
        default:
          TU_LOG2("  Unsupported recipient: %d\r\n",
                  p_request->bmRequestType_bit.recipient);
          TU_BREAKPOINT();
          return false;
      }

      // If we end here, the received request is a set request - we schedule a
      // receive for the data stage and return true here. We handle the rest
      // later in audiod_control_complete() once the data stage was finished
      TU_VERIFY(tud_control_xfer(rhport, p_request,
                                 audiod_fct_[func_id].ctrl_buf.data(),
                                 audiod_fct_[func_id].ctrl_buf_sz));
      return true;
    }

    // There went something wrong - unsupported control request type
    TU_BREAKPOINT();
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

    TU_LOG2("  Get itf: %u - current alt: %u\r\n", itf,
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

  bool audiod_set_interface(uint8_t rhport,
                            tusb_control_request_t const* p_request) {
    (void)rhport;

    // Here we need to do the following:

    // 1. Find the audio driver assigned to the given interface to be set
    // Since one audio driver interface has to be able to cover an unknown
    // number of interfaces (AC, AS + its alternate settings), the best memory
    // efficient way to solve this is to always search through the descriptors.
    // The audio driver is mapped to an audio function by a reference pointer to
    // the corresponding AC interface of this audio function which serves as a
    // starting point for searching

    // 2. Close EPs which are currently open
    // To do so it is not necessary to know the current active alternate
    // interface since we already save the current EP addresses - we simply
    // close them

    // 3. Open new EP

    uint8_t const itf = tu_u16_low(p_request->wIndex);
    uint8_t const alt = tu_u16_low(p_request->wValue);

    TU_LOG2("  Set itf: %u - alt: %u\r\n", itf, alt);

    // Find index of audio streaming interface and index of interface
    uint8_t func_id, idxItf;
    uint8_t const* p_desc;
    if (!audiod_get_AS_interface_index_global(itf, &func_id, &idxItf,
                                              &p_desc)) {
      // Unknown interface — shouldn't happen with a valid descriptor, but still
      // send the status ZLP so the host doesn't time out (-110 ETIMEDOUT).
      // On ESP32's pre-compiled TinyUSB returning false causes no response.
      tud_control_status(rhport, p_request);
      return true;
    }

    audiod_function_t* audio = &audiod_fct_[func_id];

    // Look if there is an EP to be closed - for this driver, there are only 3
    // possible EPs which may be closed (only AS related EPs can be closed, AC
    // EP (if present) is always open)
    if (getEnableEpIn()) {
      if (audio->ep_in_as_intf_num == itf) {
        audio->ep_in_as_intf_num = 0;
#ifndef TUP_DCD_EDPT_ISO_ALLOC
        usbd_edpt_close(rhport, audio->ep_in);
#endif

        // Clear FIFO since data is no longer valid
        tu_fifo_clear(&audio->ep_in_ff);

        // Invoke callback - can be used to stop data sampling
        if (tud_audio_set_itf_close_EP_cb_) {
          tud_audio_set_itf_close_EP_cb_(this, rhport, p_request);
        }

        audio->ep_in = 0;  // Necessary?

        if (getEnableEpInFlowControl()) {
          audio->packet_sz_tx[0] = 0;
          audio->packet_sz_tx[1] = 0;
          audio->packet_sz_tx[2] = 0;
        }
      }
    }  // getEnableEpIn()

    if (getEnableEpOut()) {
      if (audio->ep_out_as_intf_num == itf) {
        audio->ep_out_as_intf_num = 0;
#ifndef TUP_DCD_EDPT_ISO_ALLOC
        usbd_edpt_close(rhport, audio->ep_out);
#endif

        // Clear FIFOs, since data is no longer valid
        tu_fifo_clear(&audio->ep_out_ff);

        // Invoke callback - can be used to stop data sampling
        if (tud_audio_set_itf_close_EP_cb_) {
          tud_audio_set_itf_close_EP_cb_(this, rhport, p_request);
        }

        audio->ep_out = 0;  // Necessary?

        // Close corresponding feedback EP
        if (getEnableFeedbackEp()) {
          // #ifndef TUP_DCD_EDPT_ISO_ALLOC
          //           usbd_edpt_close(rhport, audio->ep_fb);
          // #endif
          audio->ep_fb = 0;
          tu_memclr(&audio->feedback, sizeof(audio->feedback));
        }
      }
    }  // getEnableEpOut()

    // Save current alternative interface setting
    audio->alt_setting[idxItf] = alt;

    // Acknowledge SET_INTERFACE now so the host doesn't time out (-110
    // ETIMEDOUT) if any EP-activation step below fails or returns early.
    // On ESP32's pre-compiled TinyUSB, returning false from control_xfer_cb
    // produces no response; sending the ZLP here guarantees the host always
    // gets one regardless of what happens in the activation loop below.
    tud_control_status(rhport, p_request);

    // Open new EP if necessary - EPs are only to be closed or opened for AS
    // interfaces - Look for AS interface with correct alternate interface Get
    // pointer at end
    uint8_t const* p_desc_end =
        audio->p_desc + audio->desc_length - TUD_AUDIO_DESC_IAD_LEN;

    // p_desc starts at required interface with alternate setting zero
    // Condition modified from p_desc < p_desc_end to prevent gcc>=12
    // strict-overflow warning
    while (p_desc_end - p_desc > 0) {
      // Find correct interface
      if (tu_desc_type(p_desc) == TUSB_DESC_INTERFACE &&
          ((tusb_desc_interface_t const*)p_desc)->bInterfaceNumber == itf &&
          ((tusb_desc_interface_t const*)p_desc)->bAlternateSetting == alt) {
        uint8_t const* p_desc_parse_for_params = nullptr;
        if (getEnableEpIn() && getEnableEpInFlowControl()) {
          p_desc_parse_for_params = p_desc;
        }
        // From this point forward follow the EP descriptors associated to the
        // current alternate setting interface - Open EPs if necessary
        uint8_t foundEPs = 0,
                nEps = ((tusb_desc_interface_t const*)p_desc)->bNumEndpoints;
        // Condition modified from p_desc < p_desc_end to prevent gcc>=12
        // strict-overflow warning
        while (foundEPs < nEps && (p_desc_end - p_desc > 0)) {
          if (tu_desc_type(p_desc) == TUSB_DESC_ENDPOINT) {
            tusb_desc_endpoint_t const* desc_ep =
                (tusb_desc_endpoint_t const*)p_desc;
#ifdef TUP_DCD_EDPT_ISO_ALLOC
            TU_ASSERT(usbd_edpt_iso_activate(rhport, desc_ep));
#else
            TU_ASSERT(usbd_edpt_open(rhport, desc_ep));
#endif
            uint8_t const ep_addr = desc_ep->bEndpointAddress;

            // TODO: We need to set EP non busy since this is not taken care of
            // right now in ep_close() - THIS IS A WORKAROUND!
            usbd_edpt_clear_stall(rhport, ep_addr);

            if (getEnableEpIn()) {
              if (tu_edpt_dir(ep_addr) == TUSB_DIR_IN &&
                  desc_ep->bmAttributes.usage ==
                      0x00)  // Check if usage is data EP
              {
                // Save address
                audio->ep_in = ep_addr;
                audio->ep_in_as_intf_num = itf;
                audio->ep_in_sz = tu_edpt_packet_size(desc_ep);
                if (audio->ep_in_sz == 0) return true;

                // If flow control is enabled, parse for the corresponding
                // parameters - doing this here means only AS interfaces with
                // EPs get scanned for parameters
                if (getEnableEpInFlowControl()) {
                  audiod_parse_flow_control_params(audio,
                                                   p_desc_parse_for_params);
                }
                // Schedule first transmit if alternate interface is not zero
                // i.e. streaming is disabled - in case no sample data is
                // available a ZLP is loaded It is necessary to trigger this
                // here since the refill is done with an RX FIFO empty interrupt
                // which can only trigger if something was in there
                if (getUseLinearBufferTx()) {
                  // Resize staging buffer to match actual endpoint packet size
                  audio->lin_buf_in.assign(audio->ep_in_sz, 0);
                  (void)TUSB_EDPT_XFER(rhport, audio->ep_in,
                                       audio->lin_buf_in.data(),
                                       audio->ep_in_sz);
                } else {
                  (void)TUSB_EDPT_XFER_FIFO(rhport, audio->ep_in,
                                            &audio->ep_in_ff, audio->ep_in_sz);
                }
              }
            }  // getEnableEpIn()

            if (getEnableEpOut()) {
              if (tu_edpt_dir(ep_addr) ==
                  TUSB_DIR_OUT)  // Checking usage not necessary
              {
                // Save address
                audio->ep_out = ep_addr;
                audio->ep_out_as_intf_num = itf;
                audio->ep_out_sz = tu_edpt_packet_size(desc_ep);
                if (audio->ep_out_sz == 0) return true;

                // Prepare for incoming data
                if (getUseLinearBufferRx()) {
                  // Resize staging buffer to match actual endpoint packet size
                  audio->lin_buf_out.assign(audio->ep_out_sz, 0);
                  (void)TUSB_EDPT_XFER(rhport, audio->ep_out,
                                       audio->lin_buf_out.data(),
                                       audio->ep_out_sz);
                } else {
                  (void)TUSB_EDPT_XFER_FIFO(rhport, audio->ep_out,
                                            &audio->ep_out_ff,
                                            audio->ep_out_sz);
                }
              }

              if (getEnableFeedbackEp()) {
                if (tu_edpt_dir(ep_addr) == TUSB_DIR_IN &&
                    desc_ep->bmAttributes.usage ==
                        1)  // Check if usage is explicit data feedback
                {
                  audio->ep_fb = ep_addr;
                  audio->feedback.frame_shift = desc_ep->bInterval - 1;
                }
              }
            }  // getEnableEpOut()

            foundEPs += 1;
          }
          p_desc = tu_desc_next(p_desc);
        }

        if (foundEPs != nEps) {
          TU_LOG1("  UAC2 SET_INTERFACE: foundEPs=%u nEps=%u mismatch\r\n",
                  foundEPs, nEps);
          return true;  // ZLP already sent above; don't return false
        }

        // Invoke one callback for a final set interface
        if (tud_audio_set_itf_cb_) {
          tud_audio_set_itf_cb_(this, rhport, p_request);
        }

        if (getEnableFeedbackEp()) {
          // Prepare feedback computation if endpoint is available
          if (audio->ep_fb != 0) {
            // Default: FIFO-count feedback at the configured sample rate.
            audio_feedback_params_t fb_param = {};
            fb_param.method = AUDIO_FEEDBACK_METHOD_FIFO_COUNT;
            fb_param.sample_freq = config_.sample_rate;

            if (tud_audio_feedback_params_cb_) {
              tud_audio_feedback_params_cb_(this, func_id, alt, &fb_param);
            }
            audio->feedback.compute_method = fb_param.method;

            if (TUSB_SPEED_FULL == tud_speed_get())
              if (tud_audio_feedback_format_correction_cb_) {
                audio->feedback.format_correction =
                    tud_audio_feedback_format_correction_cb_(this, func_id);
              }

            // Minimal/Maximum value in 16.16 format for full speed (1ms per
            // frame) or high speed (125 us per frame)
            uint32_t const frame_div =
                (TUSB_SPEED_FULL == tud_speed_get()) ? 1000 : 8000;
            audio->feedback.min_value = ((fb_param.sample_freq - 1) / frame_div)
                                        << 16;
            audio->feedback.max_value = (fb_param.sample_freq / frame_div + 1)
                                        << 16;

            switch (fb_param.method) {
              case AUDIO_FEEDBACK_METHOD_FREQUENCY_FIXED:
              case AUDIO_FEEDBACK_METHOD_FREQUENCY_FLOAT:
              case AUDIO_FEEDBACK_METHOD_FREQUENCY_POWER_OF_2:
                audiod_set_fb_params_freq(audio, fb_param.sample_freq,
                                          fb_param.frequency.mclk_freq);
                break;

              case AUDIO_FEEDBACK_METHOD_FIFO_COUNT: {
                // Initialize the threshold level to half filled
                uint16_t fifo_lvl_thr = tu_fifo_depth(&audio->ep_out_ff) / 2;
                audio->feedback.compute.fifo_count.fifo_lvl_thr = fifo_lvl_thr;
                audio->feedback.compute.fifo_count.fifo_lvl_avg =
                    ((uint32_t)fifo_lvl_thr) << 16;
                // Avoid 64bit division
                uint32_t nominal =
                    ((fb_param.sample_freq / 100) << 16) / (frame_div / 100);
                audio->feedback.compute.fifo_count.nom_value = nominal;
                audio->feedback.compute.fifo_count.rate_const[0] =
                    (uint16_t)((audio->feedback.max_value - nominal) /
                               fifo_lvl_thr);
                audio->feedback.compute.fifo_count.rate_const[1] =
                    (uint16_t)((nominal - audio->feedback.min_value) /
                               fifo_lvl_thr);
                // On HS feedback is more sensitive since packet size can vary
                // every MSOF, could cause instability
                if (tud_speed_get() == TUSB_SPEED_HIGH) {
                  audio->feedback.compute.fifo_count.rate_const[0] /= 8;
                  audio->feedback.compute.fifo_count.rate_const[1] /= 8;
                }
              } break;

              // nothing to do
              default:
                break;
            }
          }
        }  // getEnableFeedbackEp()

        // We are done - abort loop
        break;
      }

      // Moving forward
      p_desc = tu_desc_next(p_desc);
    }

    if (getEnableFeedbackEp()) {
      // Enable SOF interrupt whenever any feedback EP is active (all methods
      // need it)
      bool enable_sof = false;
      for (uint8_t i = 0; i < getAudioCount(); i++) {
        if (audiod_fct_[i].ep_fb != 0) {
          enable_sof = true;
          break;
        }
      }
      usbd_sof_enable(rhport, SOF_CONSUMER_AUDIO, enable_sof);
    }

    if (getEnableEpIn() && getEnableEpInFlowControl()) {
      audiod_calc_tx_packet_sz(audio);
    }

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
      TU_LOG1("  UAC2 feedback interval too small\r\n");
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
  return audio_tools::USBAudioDeviceBase::activeInstance().usbd_app_driver_get(
      count);
}
