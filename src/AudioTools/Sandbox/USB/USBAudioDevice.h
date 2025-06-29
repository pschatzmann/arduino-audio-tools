#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <mutex>
#include <vector>

#ifndef ARDUINO_USB_MODE
#error This ESP32 SoC has no Native USB interface
#else
#if ARDUINO_USB_MODE == 1
#error This sketch should be used when USB is in OTG mode
#endif
#endif


#include "USBAudio2DescriptorBuilder.h"
#include "USBAudioConfig.h"

extern "C" {
#include "device/usbd.h"
#include "device/usbd_pvt.h"
#include "tusb.h"
}

namespace audio_tools {

class USBAudioDevice {
  enum audio_format_type_t {
    AUDIO_FORMAT_TYPE_I,
    AUDIO_FORMAT_TYPE_II,
    AUDIO_FORMAT_TYPE_III,
  };

  enum audio_feedback_method_t {
    AUDIO_FEEDBACK_METHOD_DISABLED,
    AUDIO_FEEDBACK_METHOD_FREQUENCY_FIXED,
    AUDIO_FEEDBACK_METHOD_FREQUENCY_FLOAT,
    AUDIO_FEEDBACK_METHOD_FREQUENCY_POWER_OF_2,  // For driver internal use only
    AUDIO_FEEDBACK_METHOD_FIFO_COUNT
  };

  struct audiod_function_t {
    uint8_t rhport;
    uint8_t const *p_desc;  // Pointer to Standard AC Interface Descriptor
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
  void setConfig(USBAudioConfig &cfg) { config_ = cfg; }

  // Feature flag setters/getters
  bool getEnableEpIn() const { return config_.enable_ep_in;}  
  bool getEnableEpOut() const { return config_.enable_ep_out; }
  bool getEnableFeedbackEp() const { return config_.enable_feedback_ep; }
  bool getEnableEpInFlowControl() const { return config_.enable_ep_in_flow_control;}
  bool getEnableInterruptEp() const { return config_.enable_interrupt_ep; }
  bool getEnableFifoMutex() const { return config_.enable_fifo_mutex; }

  // Audio count setter/getter: getAudioCount()
  uint8_t getAudioCount() const { return config_.audio_count; }

  // Descriptor setup (call this during USB init)
  const uint8_t *getAudioDescriptors(uint8_t itf, uint8_t alt,
                                     uint16_t *out_length) {
    // // Example: simple UAC1 streaming interface with one IN and one OUT
    // endpoint static const uint8_t desc[] = {
    //     // Interface Association Descriptor (IAD)
    //     0x08, 0x0B, 0x01, 0x01, 0x02, 0x00, 0x00, 0x00,
    //     // Standard AC Interface Descriptor
    //     0x09, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
    //     // Class-specific AC Interface Descriptor
    //     0x09, 0x24, 0x01, 0x00, 0x01, 0x09, 0x00, 0x01, 0x01,
    //     // Standard AS Interface Descriptor (alt 0 - zero bandwidth)
    //     0x09, 0x04, 0x01, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00,
    //     // Standard AS Interface Descriptor (alt 1 - operational, IN)
    //     0x09, 0x04, 0x01, 0x01, 0x01, 0x01, 0x02, 0x00, 0x00,
    //     // Class-specific AS Interface Descriptor (IN)
    //     0x07, 0x24, 0x01, 0x01, 0x01, 0x01, 0x00,
    //     // Type I Format Type Descriptor (IN)
    //     0x0B, 0x24, 0x02, 0x01, 0x01, 0x02, 0x10, 0x01, 0x44, 0xAC, 0x00,
    //     // Standard AS Isochronous Audio Data Endpoint Descriptor (IN)
    //     0x07, 0x05, 0x81, 0x01, 0x40, 0x00, 0x01,
    //     // Class-specific AS Isochronous Audio Data Endpoint Descriptor (IN)
    //     0x07, 0x25, 0x01, 0x00, 0x00, 0x00, 0x00,
    //     // Standard AS Interface Descriptor (alt 2 - operational, OUT)
    //     0x09, 0x04, 0x02, 0x01, 0x01, 0x01, 0x02, 0x00, 0x00,
    //     // Class-specific AS Interface Descriptor (OUT)
    //     0x07, 0x24, 0x01, 0x01, 0x01, 0x01, 0x00,
    //     // Type I Format Type Descriptor (OUT)
    //     0x0B, 0x24, 0x02, 0x01, 0x01, 0x02, 0x10, 0x01, 0x44, 0xAC, 0x00,
    //     // Standard AS Isochronous Audio Data Endpoint Descriptor (OUT)
    //     0x07, 0x05, 0x01, 0x01, 0x40, 0x00, 0x01,
    //     // Class-specific AS Isochronous Audio Data Endpoint Descriptor (OUT)
    //     0x07, 0x25, 0x01, 0x00, 0x00, 0x00, 0x00};
    // if (out_length) *out_length = sizeof(desc);
    // return desc;
    return descr_builder.buildDescriptor(itf, alt, out_length);
  }

  // Mount status
  bool mounted() const { return tud_mounted(); }

  // Control request handler (call from tud_control_request_cb)
  bool handleControlRequest(const tusb_control_request_t *request, void *buffer,
                            uint16_t length) {
    // Example: handle standard GET_DESCRIPTOR request for audio interface
    if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD &&
        request->bRequest == TUSB_REQ_GET_DESCRIPTOR) {
      uint16_t desc_len;

      uint8_t const itf = tu_u16_low(request->wIndex);
      uint8_t const alt = tu_u16_low(request->wValue);

      const uint8_t *desc = getAudioDescriptors(itf, alt, &desc_len);
      if (desc && buffer && length >= desc_len) {
        std::memcpy(buffer, desc, desc_len);
        return true;
      }
    }
    // TODO: handle class-specific requests (mute, volume, etc.)
    return false;
  }

  // Streaming logic: call this in your main loop or USB task
  void process() {
    // Example: check if host is ready for IN, send audio packet
    if (tud_ready()) {
      uint8_t audio_data[48] = {0};
      uint16_t bytes_written = 48;
      if (tx_callback_)
        bytes_written = tx_callback_(audio_data, sizeof(audio_data));
      write(audio_data, bytes_written);
    }
    // Example: check for OUT data
    if (tud_ready()) {
      uint8_t buf[48];
      uint16_t n = read(buf, sizeof(buf));
      if (n > 0 && rx_callback_) rx_callback_(buf, n);
    }
  }

  // Register RX callback
  void setRxCallback(std::function<void(const uint8_t *, uint16_t)> cb) {
    rx_callback_ = cb;
  }
  // Register TX callback
  void setTxCallback(std::function<uint16_t(const void *, uint16_t)> cb) {
    tx_callback_ = cb;
  }
  // Setter for interface GET request callback
  void setGetReqItfCallback(std::function<bool(USBAudioDevice *, uint8_t,
                                               tusb_control_request_t const *)>
                                cb) {
    get_req_itf_cb_ = cb;
  }
  // Setter for entity GET request callback
  void setGetReqEntityCallback(
      std::function<bool(USBAudioDevice *, uint8_t,
                         tusb_control_request_t const *)>
          cb) {
    get_req_entity_cb_ = cb;
  }
  // Setter for endpoint GET request callback
  void setGetReqEpCallback(std::function<bool(USBAudioDevice *, uint8_t,
                                              tusb_control_request_t const *)>
                               cb) {
    get_req_ep_cb_ = cb;
  }
  // Setter for feedback done callback
  void setFbDoneCallback(std::function<void(USBAudioDevice *, uint8_t)> cb) {
    fb_done_cb_ = cb;
  }
  // Setter for interrupt done callback
  void setIntDoneCallback(std::function<void(USBAudioDevice *, uint8_t)> cb) {
    int_done_cb_ = cb;
  }
  // Setter for TX done callback
  void setTxDoneCallback(
      std::function<bool(USBAudioDevice *, uint8_t, audiod_function_t *)> cb) {
    tx_done_cb_ = cb;
  }
  // Setter for RX done callback
  void setRxDoneCallback(std::function<bool(USBAudioDevice *, uint8_t,
                                            audiod_function_t *, uint16_t)> cb) {
    rx_done_cb_ = cb;
  }
  // Setter for req_entity_cb_
  void setReqEntityCallback(std::function<bool(USBAudioDevice *, uint8_t)> cb) {
    req_entity_cb_ = cb;
  }
  // Setter for tud_audio_set_itf_cb_
  void setTudAudioSetItfCallback(
      std::function<bool(USBAudioDevice *, uint8_t,
                         tusb_control_request_t const *)>cb) {
    tud_audio_set_itf_cb_ = cb;
  }
  // Setter for tud_audio_set_req_entity_cb_
  void setReqEntityCallback(
      std::function<bool(USBAudioDevice *, uint8_t,
                         tusb_control_request_t const *, uint8_t *)>
          cb) {
    tud_audio_set_req_entity_cb_ = cb;
  }
  // Setter for tud_audio_set_req_itf_cb_
  void setReqItfCallback(
      std::function<bool(USBAudioDevice *, uint8_t,
                         tusb_control_request_t const *, uint8_t *)>
          cb) {
    tud_audio_set_req_itf_cb_ = cb;
  }
  // Setter for tud_audio_set_req_ep_cb_
  void setReqEpCallback(
      std::function<bool(USBAudioDevice *, uint8_t,
                         tusb_control_request_t const *, uint8_t *)>
          cb) {
    tud_audio_set_req_ep_cb_ = cb;
  }
  // Setter for tud_audio_set_itf_close_EP_cb_
  void setItfCloseEpCallback(std::function<bool(USBAudioDevice *, uint8_t,
                                                tusb_control_request_t const *)>
                                 cb) {
    tud_audio_set_itf_close_EP_cb_ = cb;
  }
  // Setter for audiod_tx_done_cb_
  void setAudiodTxDoneCallback(
      std::function<bool(USBAudioDevice *, uint8_t, audiod_function_t *)> cb) {
    audiod_tx_done_cb_ = cb;
  }
  // Setter for tud_audio_feedback_params_cb_
  void setAudioFeedbackParamsCallback(
      std::function<void(USBAudioDevice *, uint8_t, uint8_t,
                         audio_feedback_params_t *)>
          cb) {
    tud_audio_feedback_params_cb_ = cb;
  }
  // Setter for tud_audio_feedback_format_correction_cb_
  void setAudioFeedbackFormatCorrectionCallback(
      std::function<bool(USBAudioDevice *, uint8_t)> cb) {
    tud_audio_feedback_format_correction_cb_ = cb;
  }


  static USBAudioDevice &instance() {
    static USBAudioDevice instance_;
    return instance_;
  }

   usbd_class_driver_t const *usbd_app_driver_get(uint8_t *count) {
    static usbd_class_driver_t driver;
    driver.name = "Audio";
    driver.init = [](void) { USBAudioDevice::instance().audiod_init(); };
    driver.deinit = [](void) {
      return USBAudioDevice::instance().audiod_deinit();
    };
    driver.reset = [](uint8_t rhport) {
      USBAudioDevice::instance().audiod_reset(rhport);
    };
    driver.open = [](uint8_t rhport, tusb_desc_interface_t const *itf_desc,
                     uint16_t max_len) {
      return USBAudioDevice::instance().audiod_open(rhport, itf_desc, max_len);
    };
    driver.control_xfer_cb = [](uint8_t rhport, uint8_t stage,
                                tusb_control_request_t const *request) {
      return USBAudioDevice::instance().audiod_control_xfer_cb(rhport, stage,
                                                               request);
    };
    driver.xfer_cb = [](uint8_t rhport, uint8_t ep_addr, xfer_result_t result,
                        uint32_t xferred_bytes) {
      return USBAudioDevice::instance().audiod_xfer_cb(rhport, ep_addr, result,
                                                       xferred_bytes);
    };
    driver.sof = [](uint8_t rhport, uint32_t frame_count) {
      USBAudioDevice::instance().audiod_sof_isr(rhport, frame_count);
    };
    *count = 1;
    return &driver;
  }
 
 private:
  // Only one set of member variables and methods retained
  USBAudioConfig config_;
  USBAudio2DescriptorBuilder descr_builder{config_};
  std::function<void(const uint8_t *, uint16_t)> rx_callback_;
  std::function<uint16_t(const void *, uint16_t)> tx_callback_;
  std::function<void(USBAudioDevice *, uint8_t rhport)> int_done_cb_;
  std::function<bool(USBAudioDevice *, uint8_t rhport, audiod_function_t *)>
      tx_done_cb_;
  std::function<bool(USBAudioDevice *, uint8_t rhport, audiod_function_t *,
                     uint16_t xferred_bytes)>
      rx_done_cb_;
  // Callback for interface GET requests
  std::function<bool(USBAudioDevice *, uint8_t rhport,
                     tusb_control_request_t const *)>
      get_req_itf_cb_;
  // Callback for entity GET requests
  std::function<bool(USBAudioDevice *, uint8_t rhport,
                     tusb_control_request_t const *)>
      get_req_entity_cb_;
  // Callback for endpoint GET requests
  std::function<bool(USBAudioDevice *, uint8_t rhport,
                     tusb_control_request_t const *)>
      get_req_ep_cb_;

  // Callback for feedback done event
  std::function<void(USBAudioDevice *, uint8_t func_id)> fb_done_cb_;
  std::function<bool(USBAudioDevice *, uint8_t func_id)> req_entity_cb_;
  std::function<bool(USBAudioDevice *, uint8_t rhport,
                     tusb_control_request_t const *p_request)>
      tud_audio_set_itf_cb_;
  std::function<bool(USBAudioDevice *, uint8_t rhport,
                     tusb_control_request_t const *p_request, uint8_t *pBuff)>
      tud_audio_set_req_entity_cb_;

  std::function<bool(USBAudioDevice *, uint8_t rhport,
                     tusb_control_request_t const *p_request, uint8_t *pBuff)>
      tud_audio_set_req_itf_cb_;

  std::function<bool(USBAudioDevice *, uint8_t rhport,
                     tusb_control_request_t const *p_request, uint8_t *pBuff)>
      tud_audio_set_req_ep_cb_;

  std::function<bool(USBAudioDevice *, uint8_t rhport,
                     tusb_control_request_t const *p_request)>
      tud_audio_set_itf_close_EP_cb_;

  std::function<bool(USBAudioDevice *, uint8_t rhport,
                     audiod_function_t *audio)>
      audiod_tx_done_cb_;

  std::function<void(USBAudioDevice *, uint8_t func_id, uint8_t alt_itf,
                     audio_feedback_params_t *feedback_param)>
      tud_audio_feedback_params_cb_;

  std::function<bool(USBAudioDevice *, uint8_t func_id)>
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

  USBAudioDevice() = default;

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

  // Returns whether linear buffer RX is enabled
  bool getUseLinearBufferRx() const { return config_.use_linear_buffer_rx; }

  bool getUseLinearBufferTx() const { return config_.use_linear_buffer_tx; }

  // Returns the reset size for audiod_function_t up to and including
  // ctrl_buf_sz
  static constexpr size_t getResetSize() {
    return offsetof(audiod_function_t, ctrl_buf_sz) +
           sizeof(((audiod_function_t *)0)->ctrl_buf_sz);
  }

  // Dummy for tud_audio_feedback_interval_isr (should be implemented elsewhere
  // if not available)
  void tud_audio_feedback_interval_isr(uint8_t func_id, uint32_t frame_count,
                                       uint8_t frame_shift) {
    // Implement or forward to actual USB stack as needed
  }

  // Write audio data to IN endpoint
  uint16_t write(const void *data, uint16_t len) {
    uint16_t written = tud_audio_n_write(config_.ep_in, data, len);
    return written;
    return 0;
  }

  // Read audio data from OUT endpoint
  uint16_t read(void *buffer, uint16_t bufsize) {
    return tud_audio_n_read(config_.ep_out, buffer, bufsize);
  }

  // USBD Driver API
  void audiod_init(void) {
    audiod_fct_.resize(getAudioCount());
    alloc_mutex();

    // Initialize control buffers
    for (uint8_t i = 0; i < getAudioCount(); i++) {
      audiod_function_t *audio = &audiod_fct_[i];
      // Initialize control buffers
      int size = getCtrlBufSz(i);
      audio->ctrl_buf.resize(size);
      audio->ctrl_buf_sz = size;
      // Initialize active alternate interface buffers
      audio->alt_setting.resize(config_.as_descr_count);
      // Initialize IN EP FIFO if required
      if (getEnableEpIn()) {
        audio->ep_in_sw_buf.resize(getEpInSwBufSz(i));

        tu_fifo_config(&audio->ep_in_ff,
                       audio->ep_in_sw_buf.data(),  // Use data pointer for FIFO
                       getEpInSwBufSz(i), 1, true);
        if (getEnableFifoMutex()) {
          tu_fifo_config_mutex(&audio->ep_in_ff,
                               osal_mutex_create(&ep_in_ff_mutex_wr_[i]), NULL);
        }
      }
      // Initialize linear buffers
      if (getUseLinearBufferTx()) {
        audio->lin_buf_in.resize(config_.lin_buf_in_size_per_func);
      }
      // Initialize OUT EP FIFO if required
      if (getEnableEpOut()) {
        audio->ep_out_sw_buf.resize(getEpOutSwBufSz(i));
        tu_fifo_config(&audio->ep_out_ff, audio->ep_out_sw_buf.data(),
                       getEpOutSwBufSz(i), 1, true);
        if (getEnableFifoMutex()) {
          tu_fifo_config_mutex(&audio->ep_out_ff, NULL,
                               osal_mutex_create(&ep_out_ff_mutex_rd_[i]));
        }
      }
      // Initialize linear buffers
      if (getUseLinearBufferRx()) {
        audio->lin_buf_out.resize(config_.lin_buf_in_size_per_func);
      }
      if (getEnableFeedbackEp()) {
        audio->fb_buf.resize(getEpOutSwBufSz(i));
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
      audiod_function_t *audio = &audiod_fct_[i];
      memset(audio, 0, getResetSize());
      if (getEnableEpIn()) {
        tu_fifo_clear(&audio->ep_in_ff);
      }
      if (getEnableEpOut()) {
        tu_fifo_clear(&audio->ep_out_ff);
      }
    }
  }

  uint16_t audiod_open(uint8_t rhport, tusb_desc_interface_t const *itf_desc,
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
        audiod_fct_[i].p_desc = (uint8_t const *)itf_desc;
        audiod_fct_[i].rhport = rhport;
        audiod_fct_[i].desc_length = getDescLen(i);
        if (getEnableEpIn() || getEnableEpOut() || getEnableFeedbackEp()) {
          uint8_t ep_in = 0, ep_out = 0, ep_fb = 0;
          uint16_t ep_in_size = 0, ep_out_size = 0;
          uint8_t const *p_desc = audiod_fct_[i].p_desc;
          uint8_t const *p_desc_end =
              p_desc + audiod_fct_[i].desc_length - TUD_AUDIO_DESC_IAD_LEN;
          while (p_desc_end - p_desc > 0) {
            if (tu_desc_type(p_desc) == TUSB_DESC_ENDPOINT) {
              tusb_desc_endpoint_t const *desc_ep =
                  (tusb_desc_endpoint_t const *)p_desc;
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
        if (getEnableEpIn() && getEnableEpInFlowControl()) {
          uint8_t const *p_desc = audiod_fct_[i].p_desc;
          uint8_t const *p_desc_end =
              p_desc + audiod_fct_[i].desc_length - TUD_AUDIO_DESC_IAD_LEN;
          while (p_desc_end - p_desc > 0) {
            if (tu_desc_type(p_desc) == TUSB_DESC_ENDPOINT) {
              tusb_desc_endpoint_t const *desc_ep =
                  (tusb_desc_endpoint_t const *)p_desc;
              if (desc_ep->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS &&
                  desc_ep->bmAttributes.usage == 0 &&
                  tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN) {
                audiod_fct_[i].interval_tx = desc_ep->bInterval;
              }
            } else if (tu_desc_type(p_desc) == TUSB_DESC_CS_INTERFACE &&
                       tu_desc_subtype(p_desc) ==
                           AUDIO_CS_AC_INTERFACE_OUTPUT_TERMINAL) {
              if (tu_unaligned_read16(p_desc + 4) ==
                  AUDIO_TERM_TYPE_USB_STREAMING) {
                audiod_fct_[i].bclock_id_tx = p_desc[8];
              }
            }
            p_desc = tu_desc_next(p_desc);
          }
        }
        if (getEnableInterruptEp()) {
          uint8_t const *p_desc = audiod_fct_[i].p_desc;
          uint8_t const *p_desc_end =
              p_desc + audiod_fct_[i].desc_length - TUD_AUDIO_DESC_IAD_LEN;
          while (p_desc_end - p_desc > 0) {
            if (tu_desc_type(p_desc) == TUSB_DESC_ENDPOINT) {
              tusb_desc_endpoint_t const *desc_ep =
                  (tusb_desc_endpoint_t const *)p_desc;
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
                              tusb_control_request_t const *request) {
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
                               tusb_control_request_t const *p_request) {
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
      audiod_function_t *audio = &audiod_fct_[func_id];
      if (getEnableInterruptEp() && audio->ep_int == ep_addr) {
        if (int_done_cb_) int_done_cb_(this, rhport);
        return true;
      }
      if (getEnableEpIn() && audio->ep_in == ep_addr &&
          audio->alt_setting.size() != 0) {
        if (tx_done_cb_ && tx_done_cb_(this, rhport, audio)) return true;
        return true;
      }
      if (getEnableEpOut() && audio->ep_out == ep_addr) {
        if (rx_done_cb_ &&
            rx_done_cb_(this, rhport, audio, (uint16_t)xferred_bytes))
          return true;
        return true;
      }
      if (getEnableFeedbackEp() && audio->ep_fb == ep_addr) {
        if (fb_done_cb_) {
          fb_done_cb_(this, func_id);
        }
        if (usbd_edpt_claim(rhport, audio->ep_fb)) {
          return audiod_fb_send(audio);
        }
      }
    }
    return false;
  }

  TU_ATTR_FAST_FUNC void audiod_sof_isr(uint8_t rhport, uint32_t frame_count) {
    (void)rhport;
    (void)frame_count;
    if (getEnableEpOut() && getEnableFeedbackEp()) {
      for (uint8_t i = 0; i < getAudioCount(); i++) {
        audiod_function_t *audio = &audiod_fct_[i];
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
                              tusb_control_request_t const *p_request) {
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
            if (p_request->bmRequestType_bit.direction == TUSB_DIR_IN &&
                req_entity_cb_) {
              return req_entity_cb_(this, func_id);
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
                                   uint8_t *func_id) {
    uint8_t i;
    for (i = 0; i < getAudioCount(); i++) {
      // Look for the correct driver by checking if the unique standard AC
      // interface number fits
      if (audiod_fct_[i].p_desc &&
          ((tusb_desc_interface_t const *)audiod_fct_[i].p_desc)
                  ->bInterfaceNumber == itf) {
        // Get pointers after class specific AC descriptors and end of AC
        // descriptors - entities are defined in between
        uint8_t const *p_desc =
            tu_desc_next(audiod_fct_[i].p_desc);  // Points to CS AC descriptor
        uint8_t const *p_desc_end =
            ((audio_desc_cs_ac_interface_t const *)p_desc)->wTotalLength +
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

  bool audiod_verify_ep_exists(uint8_t ep, uint8_t *func_id) {
    uint8_t i;
    for (i = 0; i < getAudioCount(); i++) {
      if (audiod_fct_[i].p_desc) {
        // Get pointer at end
        uint8_t const *p_desc_end =
            audiod_fct_[i].p_desc + audiod_fct_[i].desc_length;

        // Advance past AC descriptors - EP we look for are streaming EPs
        uint8_t const *p_desc = tu_desc_next(audiod_fct_[i].p_desc);
        p_desc += ((audio_desc_cs_ac_interface_t const *)p_desc)->wTotalLength;

        // Condition modified from p_desc < p_desc_end to prevent gcc>=12
        // strict-overflow warning
        while (p_desc_end - p_desc > 0) {
          if (tu_desc_type(p_desc) == TUSB_DESC_ENDPOINT &&
              ((tusb_desc_endpoint_t const *)p_desc)->bEndpointAddress == ep) {
            *func_id = i;
            return true;
          }
          p_desc = tu_desc_next(p_desc);
        }
      }
    }
    return false;
  }

  bool audiod_verify_itf_exists(uint8_t itf, uint8_t *func_id) {
    uint8_t i;
    for (i = 0; i < getAudioCount(); i++) {
      if (audiod_fct_[i].p_desc) {
        // Get pointer at beginning and end
        uint8_t const *p_desc = audiod_fct_[i].p_desc;
        uint8_t const *p_desc_end = audiod_fct_[i].p_desc +
                                    audiod_fct_[i].desc_length -
                                    TUD_AUDIO_DESC_IAD_LEN;
        // Condition modified from p_desc < p_desc_end to prevent gcc>=12
        // strict-overflow warning
        while (p_desc_end - p_desc > 0) {
          if (tu_desc_type(p_desc) == TUSB_DESC_INTERFACE &&
              ((tusb_desc_interface_t const *)audiod_fct_[i].p_desc)
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

  void audiod_parse_flow_control_params(audiod_function_t *audio,
                                        uint8_t const *p_desc) {
    p_desc = tu_desc_next(p_desc);  // Exclude standard AS interface descriptor
                                    // of current alternate interface descriptor

    // Look for a Class-Specific AS Interface Descriptor(4.9.2) to verify format
    // type and format and also to get number of physical channels
    if (tu_desc_type(p_desc) == TUSB_DESC_CS_INTERFACE &&
        tu_desc_subtype(p_desc) == AUDIO_CS_AS_INTERFACE_AS_GENERAL) {
      audio->n_channels_tx =
          ((audio_desc_cs_as_interface_t const *)p_desc)->bNrChannels;
      audio->format_type_tx =
          (audio_format_type_t)(((audio_desc_cs_as_interface_t const *)p_desc)
                                    ->bFormatType);
      // Look for a Type I Format Type Descriptor(2.3.1.6 - Audio Formats)
      p_desc = tu_desc_next(p_desc);
      if (tu_desc_type(p_desc) == TUSB_DESC_CS_INTERFACE &&
          tu_desc_subtype(p_desc) == AUDIO_CS_AS_INTERFACE_FORMAT_TYPE &&
          ((audio_desc_type_I_format_t const *)p_desc)->bFormatType ==
              AUDIO_FORMAT_TYPE_I) {
        audio->n_bytes_per_sample_tx =
            ((audio_desc_type_I_format_t const *)p_desc)->bSubslotSize;
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
  bool audiod_get_AS_interface_index(uint8_t itf, audiod_function_t *audio,
                                     uint8_t *idxItf,
                                     uint8_t const **pp_desc_int) {
    if (audio->p_desc) {
      // Get pointer at end
      uint8_t const *p_desc_end =
          audio->p_desc + audio->desc_length - TUD_AUDIO_DESC_IAD_LEN;

      // Advance past AC descriptors
      uint8_t const *p_desc = tu_desc_next(audio->p_desc);
      p_desc += ((audio_desc_cs_ac_interface_t const *)p_desc)->wTotalLength;

      uint8_t tmp = 0;
      // Condition modified from p_desc < p_desc_end to prevent gcc>=12
      // strict-overflow warning
      while (p_desc_end - p_desc > 0) {
        // We assume the number of alternate settings is increasing thus we
        // return the index of alternate setting zero!
        if (tu_desc_type(p_desc) == TUSB_DESC_INTERFACE &&
            ((tusb_desc_interface_t const *)p_desc)->bAlternateSetting == 0) {
          if (((tusb_desc_interface_t const *)p_desc)->bInterfaceNumber ==
              itf) {
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
  bool audiod_get_AS_interface_index_global(uint8_t itf, uint8_t *func_id,
                                            uint8_t *idxItf,
                                            uint8_t const **pp_desc_int) {
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
                            tusb_control_request_t const *p_request) {
    uint8_t const itf = tu_u16_low(p_request->wIndex);

    // Find index of audio streaming interface
    uint8_t func_id, idxItf;
    uint8_t const *dummy;

    TU_VERIFY(
        audiod_get_AS_interface_index_global(itf, &func_id, &idxItf, &dummy));
    TU_VERIFY(tud_control_xfer(rhport, p_request,
                               &audiod_fct_[func_id].alt_setting[idxItf], 1));

    TU_LOG2("  Get itf: %u - current alt: %u\r\n", itf,
            audiod_fct_[func_id].alt_setting[idxItf]);

    return true;
  }

  bool audiod_fb_send(audiod_function_t *audio) {
    bool apply_correction = (TUSB_SPEED_FULL == tud_speed_get()) &&
                            audio->feedback.format_correction;
    // Format the feedback value
    if (apply_correction) {
      uint8_t *fb = (uint8_t *)audio->fb_buf.data();

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
    return usbd_edpt_xfer(audio->rhport, audio->ep_fb,
                          (uint8_t *)audio->fb_buf.data(),
                          apply_correction ? 3 : 4);
  }

  bool audiod_set_interface(uint8_t rhport,
                            tusb_control_request_t const *p_request) {
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
    uint8_t const *p_desc;
    TU_VERIFY(
        audiod_get_AS_interface_index_global(itf, &func_id, &idxItf, &p_desc));

    audiod_function_t *audio = &audiod_fct_[func_id];

    // Look if there is an EP to be closed - for this driver, there are only 3
    // possible EPs which may be closed (only AS related EPs can be closed, AC
    // EP (if present) is always open)
    if (getEnableEpIn()) {
      if (audio->ep_in_as_intf_num == itf) {
        audio->ep_in_as_intf_num = 0;
#ifndef TUP_DCD_EDPT_ISO_ALLOC
        usbd_edpt_close(rhport, audio->ep_in);
#endif

        // Clear FIFOs, since data is no longer valid
        tu_fifo_clear(&audio->ep_in_ff);

        // Invoke callback - can be used to stop data sampling
        if (tud_audio_set_itf_close_EP_cb_) {
          TU_VERIFY(tud_audio_set_itf_close_EP_cb_(this, rhport, p_request));
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
          TU_VERIFY(tud_audio_set_itf_close_EP_cb_(this, rhport, p_request));
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

    // Open new EP if necessary - EPs are only to be closed or opened for AS
    // interfaces - Look for AS interface with correct alternate interface Get
    // pointer at end
    uint8_t const *p_desc_end =
        audio->p_desc + audio->desc_length - TUD_AUDIO_DESC_IAD_LEN;

    // p_desc starts at required interface with alternate setting zero
    // Condition modified from p_desc < p_desc_end to prevent gcc>=12
    // strict-overflow warning
    while (p_desc_end - p_desc > 0) {
      // Find correct interface
      if (tu_desc_type(p_desc) == TUSB_DESC_INTERFACE &&
          ((tusb_desc_interface_t const *)p_desc)->bInterfaceNumber == itf &&
          ((tusb_desc_interface_t const *)p_desc)->bAlternateSetting == alt) {
        uint8_t const *p_desc_parse_for_params = nullptr;
        if (getEnableEpIn() && getEnableEpInFlowControl()) {
          p_desc_parse_for_params = p_desc;
        }
        // From this point forward follow the EP descriptors associated to the
        // current alternate setting interface - Open EPs if necessary
        uint8_t foundEPs = 0,
                nEps = ((tusb_desc_interface_t const *)p_desc)->bNumEndpoints;
        // Condition modified from p_desc < p_desc_end to prevent gcc>=12
        // strict-overflow warning
        while (foundEPs < nEps && (p_desc_end - p_desc > 0)) {
          if (tu_desc_type(p_desc) == TUSB_DESC_ENDPOINT) {
            tusb_desc_endpoint_t const *desc_ep =
                (tusb_desc_endpoint_t const *)p_desc;
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
                if (audiod_tx_done_cb_) {
                  TU_VERIFY(
                      audiod_tx_done_cb_(this, rhport, &audiod_fct_[func_id]));
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

                // Prepare for incoming data
                if (getUseLinearBufferRx()) {
                  TU_VERIFY(usbd_edpt_xfer(rhport, audio->ep_out,
                                           audio->lin_buf_out.data(),
                                           audio->ep_out_sz),
                            false);
                } else {
                  TU_VERIFY(
                      usbd_edpt_xfer_fifo(rhport, audio->ep_out,
                                          &audio->ep_out_ff, audio->ep_out_sz),
                      false);
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

        TU_VERIFY(foundEPs == nEps);

        // Invoke one callback for a final set interface
        if (tud_audio_set_itf_cb_) {
          TU_VERIFY(tud_audio_set_itf_cb_(this, rhport, p_request));
        }

        if (getEnableFeedbackEp()) {
          // Prepare feedback computation if endpoint is available
          if (audio->ep_fb != 0) {
            audio_feedback_params_t fb_param;

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
      // Disable SOF interrupt if no driver has any enabled feedback EP
      bool enable_sof = false;
      for (uint8_t i = 0; i < getAudioCount(); i++) {
        if (audiod_fct_[i].ep_fb != 0 &&
            (audiod_fct_[i].feedback.compute_method ==
                 AUDIO_FEEDBACK_METHOD_FREQUENCY_FIXED ||
             audiod_fct_[i].feedback.compute_method ==
                 AUDIO_FEEDBACK_METHOD_FREQUENCY_FLOAT ||
             audiod_fct_[i].feedback.compute_method ==
                 AUDIO_FEEDBACK_METHOD_FREQUENCY_POWER_OF_2)) {
          enable_sof = true;
          break;
        }
      }
      usbd_sof_enable(rhport, SOF_CONSUMER_AUDIO, enable_sof);
    }

    if (getEnableEpIn() && getEnableEpInFlowControl()) {
      audiod_calc_tx_packet_sz(audio);
    }

    tud_control_status(rhport, p_request);

    return true;
  }

  bool audiod_set_fb_params_freq(audiod_function_t *audio, uint32_t sample_freq,
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

  bool audiod_calc_tx_packet_sz(audiod_function_t *audio) {
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

  uint16_t tud_audio_n_write(uint8_t func_id, const void *data, uint16_t len) {
    TU_VERIFY(func_id < getAudioCount() && audiod_fct_[func_id].p_desc != NULL);
    return tu_fifo_write_n(&audiod_fct_[func_id].ep_in_ff, data, len);
  }

  uint16_t tud_audio_n_available(uint8_t func_id) {
    TU_VERIFY(func_id < getAudioCount() && audiod_fct_[func_id].p_desc != NULL);
    return tu_fifo_count(&audiod_fct_[func_id].ep_out_ff);
  }

  uint16_t tud_audio_n_read(uint8_t func_id, void *buffer, uint16_t bufsize) {
    TU_VERIFY(func_id < getAudioCount() && audiod_fct_[func_id].p_desc != NULL);
    return tu_fifo_read_n(&audiod_fct_[func_id].ep_out_ff, buffer, bufsize);
  }
};

}  // namespace audio_tools

// Custom driver registration
extern usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *count) {
  return audio_tools::USBAudioDevice::instance().usbd_app_driver_get(count);
}
