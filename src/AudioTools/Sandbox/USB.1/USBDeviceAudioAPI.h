#pragma once

#include "class/audio/audio.h"
#include "common/tusb_mcu.h"
#include "common/tusb_verify.h"
#include "device/usbd.h"
#include "device/usbd_pvt.h"
#include "osal/osal.h"
#include "tusb.h"
#include "tusb_option.h"
#include "vector"

#undef OUT_SW_BUF_MEM_SECTION
#undef CFG_TUSB_MEM_ALIGN
#define OUT_SW_BUF_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN

class USBDeviceAudio;

enum {
  AUDIO_FEEDBACK_METHOD_DISABLED,
  AUDIO_FEEDBACK_METHOD_FREQUENCY_FIXED,
  AUDIO_FEEDBACK_METHOD_FREQUENCY_FLOAT,
  AUDIO_FEEDBACK_METHOD_FREQUENCY_POWER_OF_2,  // For driver internal use only
  AUDIO_FEEDBACK_METHOD_FIFO_COUNT
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

/**
 * @brief Configuration for TinyUSB Audio
 */
class USBAudioConfig {
 public:
  int rh_port = 0;
  uint8_t channels = 2;
  uint32_t sample_rate = 48000;
  uint8_t bits_per_sample = 16;
  bool enable_feedback_ep = true;
  bool enable_interrupt_ep = true;
  bool enable_feedback_forward_correction = false;
  bool enable_feedback_interval_isr = false;
  bool enable_ep_in_flow_control = true;
  bool enable_linear_buffer_tx = true;
  bool enable_linear_buffer_rx = true;
  bool enable_fifo_mutex = CFG_FIFO_MUTEX;
  int func_n_as_int = 1;
  int func_ctl_buffer_size = 0;
  int func_ep_in_sw_buffer_size = 0;
  int func_ep_out_sw_buffer_size = 0;
  int func_ep_in_size_max = 0;   // CFG_TUD_AUDIO_EP_SZ_IN
  int func_ep_out_size_max = 0;  // CFG_TUD_AUDIO_EP_SZ_OUT
  size_t (*p_write_callback)(const uint8_t *data, size_t len,
                             USBDeviceAudio &ref) = nullptr;
  size_t (*p_read_callback)(uint8_t *data, size_t len,
                            USBDeviceAudio &ref) = nullptr;

  bool is_ep_out() { return p_write_callback != nullptr; }
  bool is_ep_in() { return p_read_callback != nullptr; };

  // setup (missing) default values
  void begin() {
    if (func_ctl_buffer_size == 0) func_ctl_buffer_size = 64;
    if (func_ep_in_size_max == 0)
      func_ep_in_size_max =
          TUD_AUDIO_EP_SIZE(sample_rate, bits_per_sample / 8, channels);
    if (func_ep_out_size_max == 0)
      func_ep_out_size_max =
          TUD_AUDIO_EP_SIZE(sample_rate, bits_per_sample / 8, channels);
    if (func_ep_out_size_max == 0)
      func_ep_in_sw_buffer_size =
          (TUD_OPT_HIGH_SPEED ? 32 : 4) *
          func_ep_in_size_max;  // Example write FIFO every 1ms, so it should be
                                // 8 times larger for HS device
    if (func_ep_out_sw_buffer_size == 0)
      func_ep_out_sw_buffer_size =
          (TUD_OPT_HIGH_SPEED ? 32 : 4) *
          func_ep_out_size_max;  // Example write FIFO every 1ms, so it should
                                 // be 8 times larger for HS device
  }
  void clear() {
    func_ctl_buffer_size = 0;
    func_ep_in_size_max = 0;
    func_ep_out_size_max = 0;
    func_ep_in_sw_buffer_size = 0;
    func_ep_out_sw_buffer_size = 0;
  }
};

/***
 * @brief Basic TinyUSB Audio User Callbacks
 */
class USBAudioCB {
 public:
  USBAudioCB() = default;
  virtual uint16_t getInterfaceDescriptor(uint8_t itfnum, uint8_t *buf,
                                          uint16_t bufsize) = 0;

  virtual size_t getInterfaceDescriptorLength(uint8_t itfnum) = 0;

  // Invoked when set interface is called, typically on start/stop streaming or
  // format change
  virtual bool set_itf_cb(uint8_t rhport,
                          tusb_control_request_t const *p_request) = 0;

  // Invoked when audio class specific set request received for an EP
  virtual bool set_req_ep_cb(uint8_t rhport,
                             tusb_control_request_t const *p_request,
                             uint8_t *pBuff) = 0;

  // Invoked when audio class specific set request received for an interface
  virtual bool set_req_itf_cb(uint8_t rhport,
                              tusb_control_request_t const *p_request,
                              uint8_t *pBuff) = 0;

  // Invoked when audio class specific set request received for an entity
  virtual bool set_req_entity_cb(uint8_t rhport,
                                 tusb_control_request_t const *p_request,
                                 uint8_t *pBuff) = 0;
  // Invoked when audio class specific get request received for an EP
  virtual bool get_req_ep_cb(uint8_t rhport,
                             tusb_control_request_t const *p_request) = 0;

  // Invoked when audio class specific get request received for an interface
  virtual bool get_req_itf_cb(uint8_t rhport,
                              tusb_control_request_t const *p_request) = 0;

  // Invoked when audio class specific get request received for an entity
  virtual bool get_req_entity_cb(uint8_t rhport,
                                 tusb_control_request_t const *p_request) = 0;
  virtual bool tx_done_pre_load_cb(uint8_t rhport, uint8_t itf, uint8_t ep_in,
                                   uint8_t cur_alt_setting) = 0;

  virtual bool tx_done_post_load_cb(uint8_t rhport, uint16_t n_bytes_copied,
                                    uint8_t itf, uint8_t ep_in,
                                    uint8_t cur_alt_setting) = 0;

  virtual bool rx_done_pre_read_cb(uint8_t rhport, uint16_t n_bytes_received,
                                   uint8_t func_id, uint8_t ep_out,
                                   uint8_t cur_alt_setting) = 0;
  virtual bool rx_done_post_read_cb(uint8_t rhport, uint16_t n_bytes_received,
                                    uint8_t func_id, uint8_t ep_out,
                                    uint8_t cur_alt_setting) = 0;

  virtual bool set_itf_close_EP_cb(uint8_t rhport,
                                   tusb_control_request_t const *p_request) = 0;

  // for speaker
  virtual void feedback_params_cb(uint8_t func_id, uint8_t alt_itf,
                                  audio_feedback_params_t *feedback_param) = 0;

  virtual void int_done_cb(uint8_t rhport) {};
  virtual void fb_done_cb(uint8_t func_id) {};
  virtual void feedback_interval_isr(uint8_t func_id, uint32_t frame_number,
                                     uint8_t interval_shift) {}

  virtual uint8_t allocInterface(uint8_t count = 1) = 0;
  virtual uint8_t allocEndpoint(uint8_t in) = 0;

  int func_id = 0;
};

/***
 * @brief Baisc TinyUSB Audio Device Driver as C++ class which does not rely on
 * preprocesser defines!.
 */
class USBDeviceAudioAPI {
 public:
  bool tud_audio_n_mounted(uint8_t func_id) {
    TU_VERIFY(func_id < cfg.func_n_as_int);
    audiod_function_t *audio = &_audiod_fct[func_id];

    return audio->mounted;
  }

  //--------------------------------------------------------------------+
  // READ API
  //--------------------------------------------------------------------+

  uint16_t tud_audio_n_available(uint8_t func_id) {
    TU_VERIFY(func_id < cfg.func_n_as_int &&
              _audiod_fct[func_id].p_desc != NULL);
    return tu_fifo_count(&_audiod_fct[func_id].ep_out_ff);
  }

  uint16_t tud_audio_n_read(uint8_t func_id, void *buffer, uint16_t bufsize) {
    TU_VERIFY(func_id < cfg.func_n_as_int &&
              _audiod_fct[func_id].p_desc != NULL);
    return tu_fifo_read_n(&_audiod_fct[func_id].ep_out_ff, buffer, bufsize);
  }

  bool tud_audio_n_clear_ep_out_ff(uint8_t func_id) {
    TU_VERIFY(func_id < cfg.func_n_as_int &&
              _audiod_fct[func_id].p_desc != NULL);
    return tu_fifo_clear(&_audiod_fct[func_id].ep_out_ff);
  }

  tu_fifo_t *tud_audio_n_get_ep_out_ff(uint8_t func_id) {
    if (func_id < cfg.func_n_as_int && _audiod_fct[func_id].p_desc != NULL)
      return &_audiod_fct[func_id].ep_out_ff;
    return NULL;
  }

  //--------------------------------------------------------------------+
  // WRITE API
  //--------------------------------------------------------------------+

  /**
   * \brief           Write data to EP in buffer
   *
   *  Write data to buffer. If it is full, new data can be inserted once a
   * transmit was scheduled. See audiod_tx_done_cb(). If TX FIFOs are used, this
   * function is not available in order to not let the user mess up the encoding
   * process.
   *
   * \param[in]       func_id: Index of audio function interface
   * \param[in]       data: Pointer to data array to be copied from
   * \param[in]       len: # of array elements to copy
   * \return          Number of bytes actually written
   */
  uint16_t tud_audio_n_write(uint8_t func_id, const void *data, uint16_t len) {
    TU_VERIFY(func_id < cfg.func_n_as_int &&
              _audiod_fct[func_id].p_desc != NULL);
    return tu_fifo_write_n(&_audiod_fct[func_id].ep_in_ff, data, len);
  }

  bool tud_audio_n_clear_ep_in_ff(
      uint8_t func_id)  // Delete all content in the EP IN FIFO
  {
    TU_VERIFY(func_id < cfg.func_n_as_int &&
              _audiod_fct[func_id].p_desc != NULL);
    return tu_fifo_clear(&_audiod_fct[func_id].ep_in_ff);
  }

  tu_fifo_t *tud_audio_n_get_ep_in_ff(uint8_t func_id) {
    if (func_id < cfg.func_n_as_int && _audiod_fct[func_id].p_desc != NULL)
      return &_audiod_fct[func_id].ep_in_ff;
    return NULL;
  }

  // If no interrupt transmit is pending bytes get written into buffer and a
  // transmit is scheduled - once transmit completed tud_audio_int_done_cb() is
  // called in inform user
  bool tud_audio_int_n_write(uint8_t func_id,
                             const audio_interrupt_data_t *data) {
    TU_VERIFY(func_id < cfg.func_n_as_int &&
              _audiod_fct[func_id].p_desc != NULL);

    TU_VERIFY(_audiod_fct[func_id].ep_int != 0);

    // We write directly into the EP's buffer - abort if previous transfer not
    // complete
    TU_VERIFY(usbd_edpt_claim(_audiod_fct[func_id].rhport,
                              _audiod_fct[func_id].ep_int));

    // Check length
    if (tu_memcpy_s(_audiod_fct[func_id].ep_int_buf,
                    sizeof(_audiod_fct[func_id].ep_int_buf), data,
                    sizeof(audio_interrupt_data_t)) == 0) {
      // Schedule transmit
      TU_ASSERT(usbd_edpt_xfer(_audiod_fct[func_id].rhport,
                               _audiod_fct[func_id].ep_int,
                               _audiod_fct[func_id].ep_int_buf,
                               sizeof(_audiod_fct[func_id].ep_int_buf)),
                0);
    } else {
      // Release endpoint since we don't make any transfer
      usbd_edpt_release(_audiod_fct[func_id].rhport,
                        _audiod_fct[func_id].ep_int);
    }

    return true;
  }

  //--------------------------------------------------------------------+
  // USBD Driver API
  //--------------------------------------------------------------------+
  void begin(USBAudioCB *cb, USBAudioConfig config) {
    p_cb = cb;
    cfg = config;
    cfg.begin();
  }

  void audiod_init() {
    if (p_cb == nullptr) return;
    _audiod_fct.resize(cfg.func_n_as_int);
    ctrl_buf_1.resize(cfg.func_ctl_buffer_size);
    alt_setting_1.resize(cfg.func_n_as_int);

    if (cfg.is_ep_in()) {
      if (cfg.enable_linear_buffer_rx)
        lin_buf_in_1.resize(cfg.func_ep_in_size_max);
      audio_ep_in_sw_buf_1.resize(cfg.func_ep_in_sw_buffer_size);
    }

    if (cfg.is_ep_out()) {
      if (cfg.enable_linear_buffer_tx)
        audio_ep_out_sw_buf_1.resize(cfg.func_ep_out_sw_buffer_size);
      lin_buf_out_1.resize(cfg.func_ep_out_sw_buffer_size);
    }

    audiod_function_t *audio = &_audiod_fct[0];

    audio->ctrl_buf = ctrl_buf_1.data();
    audio->ctrl_buf_sz = cfg.func_ctl_buffer_size;
    audio->alt_setting = alt_setting_1.data();

    // Initialize IN EP FIFO if required
    if (cfg.is_ep_in()) {
      tu_fifo_config(&audio->ep_in_ff, audio_ep_in_sw_buf_1.data(),
                     cfg.func_ep_in_sw_buffer_size, 1, true);
      if (cfg.enable_fifo_mutex)
        tu_fifo_config_mutex(&audio->ep_in_ff,
                             osal_mutex_create(&ep_in_ff_mutex_wr_1), NULL);
    }
    // cfg.is_ep_in() && !ENABLE_ENCODING

    // Initialize linear buffers
    if (cfg.enable_linear_buffer_tx) audio->lin_buf_in = lin_buf_in_1.data();

    // Initialize OUT EP FIFO if required
    if (cfg.is_ep_out()) {
      tu_fifo_config(&audio->ep_out_ff, audio_ep_out_sw_buf_1.data(),
                     cfg.func_ep_in_sw_buffer_size, 1, true);
      if (cfg.enable_fifo_mutex)
        tu_fifo_config_mutex(&audio->ep_out_ff, NULL,
                             osal_mutex_create(&ep_out_ff_mutex_rd_1));
    }

    // Initialize linear buffers
    if (cfg.enable_linear_buffer_rx) audio->lin_buf_out = lin_buf_out_1.data();
  }

  bool audiod_deinit(void) { return true; }

  void audiod_reset(uint8_t rhport) {
    (void)rhport;

    for (uint8_t i = 0; i < cfg.func_n_as_int; i++) {
      audiod_function_t *audio = &_audiod_fct[i];
      tu_memclr(audio, sizeof(audiod_function_t));

      if (cfg.is_ep_in()) tu_fifo_clear(&audio->ep_in_ff);

      if (cfg.is_ep_out()) tu_fifo_clear(&audio->ep_out_ff);
    }
  }

  uint16_t audiod_open(uint8_t rhport, tusb_desc_interface_t const *itf_desc,
                       uint16_t max_len) {
    (void)max_len;
    if (p_cb == nullptr) return 0;

    int cls_tobe = TUSB_CLASS_AUDIO;
    int cls_is = itf_desc->bInterfaceClass;

    // TU_VERIFY(TUSB_CLASS_AUDIO == itf_desc->bInterfaceClass &&
    //           AUDIO_SUBCLASS_CONTROL == itf_desc->bInterfaceSubClass);

    // // Verify version is correct - this check can be omitted
    // TU_VERIFY(itf_desc->bInterfaceProtocol == AUDIO_INT_PROTOCOL_CODE_V2);

    // Verify interrupt control EP is enabled if demanded by descriptor
    TU_ASSERT(itf_desc->bNumEndpoints <= 1);  // 0 or 1 EPs are allowed
    if (itf_desc->bNumEndpoints == 1) {
      TU_ASSERT(cfg.enable_interrupt_ep);
    }

    // Alternate setting MUST be zero - this check can be omitted
    TU_VERIFY(itf_desc->bAlternateSetting == 0);

    // Find available audio driver interface
    uint8_t i;
    for (i = 0; i < cfg.func_n_as_int; i++) {
      if (!_audiod_fct[i].p_desc) {
        int len = p_cb->getInterfaceDescriptor(i, nullptr, 0);
        _audiod_fct[i].desc_length = len;
        descriptor.resize(len);
        _audiod_fct[i].p_desc = descriptor.data();
        p_cb->getInterfaceDescriptor(i, descriptor.data(), len);
        _audiod_fct[i].rhport = rhport;

#ifdef TUP_DCD_EDPT_ISO_ALLOC
        {
          uint8_t ep_in = 0;
          uint16_t ep_in_size = 0;

          uint8_t ep_out = 0;
          uint16_t ep_out_size = 0;

          uint8_t ep_fb = 0;
          uint8_t const *p_desc = _audiod_fct[i].p_desc;
          uint8_t const *p_desc_end =
              p_desc + _audiod_fct[i].desc_length - TUD_AUDIO_DESC_IAD_LEN;
          // Condition modified from p_desc < p_desc_end to prevent gcc>=12
          // strict-overflow warning
          while (p_desc_end - p_desc > 0) {
            if (tu_desc_type(p_desc) == TUSB_DESC_ENDPOINT) {
              tusb_desc_endpoint_t const *desc_ep =
                  (tusb_desc_endpoint_t const *)p_desc;
              if (desc_ep->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS) {
                if (cfg.enable_feedback_ep) {
                  // Explicit feedback EP
                  if (desc_ep->bmAttributes.usage == 1) {
                    ep_fb = desc_ep->bEndpointAddress;
                  }
                }
                // Data EP
                if (desc_ep->bmAttributes.usage == 0) {
                  if (tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN) {
                    if (cfg.is_ep_in()) {
                      ep_in = desc_ep->bEndpointAddress;
                      ep_in_size =
                          TU_MAX(tu_edpt_packet_size(desc_ep), ep_in_size);
                    }
                  } else {
                    if (cfg.is_ep_out()) {
                      ep_out = desc_ep->bEndpointAddress;
                      ep_out_size =
                          TU_MAX(tu_edpt_packet_size(desc_ep), ep_out_size);
                    }
                  }
                }
              }
            }

            p_desc = tu_desc_next(p_desc);
          }

          if (cfg.is_ep_in() && ep_in) {
            usbd_edpt_iso_alloc(rhport, ep_in, ep_in_size);
          }


          if (cfg.is_ep_out() && ep_out) {
            usbd_edpt_iso_alloc(rhport, ep_out, ep_out_size);
          }

          if (cfg.enable_feedback_ep) {
            if (ep_fb) {
              usbd_edpt_iso_alloc(rhport, ep_fb, 4);
            }
          }
        }

#endif  // TUP_DCD_EDPT_ISO_ALLOC

        if (cfg.is_ep_in() && cfg.enable_ep_in_flow_control) {
          uint8_t const *p_desc = _audiod_fct[i].p_desc;
          uint8_t const *p_desc_end =
              p_desc + _audiod_fct[i].desc_length - TUD_AUDIO_DESC_IAD_LEN;
          // Condition modified from p_desc < p_desc_end to prevent gcc>=12
          // strict-overflow warning
          while (p_desc_end - p_desc > 0) {
            if (tu_desc_type(p_desc) == TUSB_DESC_ENDPOINT) {
              tusb_desc_endpoint_t const *desc_ep =
                  (tusb_desc_endpoint_t const *)p_desc;
              if (desc_ep->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS) {
                if (desc_ep->bmAttributes.usage == 0) {
                  if (tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN) {
                    _audiod_fct[i].interval_tx = desc_ep->bInterval;
                  }
                }
              }
            } else if (tu_desc_type(p_desc) == TUSB_DESC_CS_INTERFACE &&
                       tu_desc_subtype(p_desc) ==
                           AUDIO_CS_AC_INTERFACE_OUTPUT_TERMINAL) {
              if (tu_unaligned_read16(p_desc + 4) ==
                  AUDIO_TERM_TYPE_USB_STREAMING) {
                _audiod_fct[i].bclock_id_tx = p_desc[8];
              }
            }
            p_desc = tu_desc_next(p_desc);
          }
        }  // CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL

        if (cfg.enable_interrupt_ep) {
          uint8_t const *p_desc = _audiod_fct[i].p_desc;
          uint8_t const *p_desc_end =
              p_desc + _audiod_fct[i].desc_length - TUD_AUDIO_DESC_IAD_LEN;
          // Condition modified from p_desc < p_desc_end to prevent gcc>=12
          // strict-overflow warning
          while (p_desc_end - p_desc > 0) {
            // For each endpoint
            if (tu_desc_type(p_desc) == TUSB_DESC_ENDPOINT) {
              tusb_desc_endpoint_t const *desc_ep =
                  (tusb_desc_endpoint_t const *)p_desc;
              uint8_t const ep_addr = desc_ep->bEndpointAddress;
              // If endpoint is input-direction and interrupt-type
              if (tu_edpt_dir(ep_addr) == TUSB_DIR_IN &&
                  desc_ep->bmAttributes.xfer == TUSB_XFER_INTERRUPT) {
                // Store endpoint number and open endpoint
                _audiod_fct[i].ep_int = ep_addr;
                TU_ASSERT(usbd_edpt_open(_audiod_fct[i].rhport, desc_ep));
              }
            }
            p_desc = tu_desc_next(p_desc);
          }
        }

        _audiod_fct[i].mounted = true;
        break;
      }
    }

    // Verify we found a free one
    TU_ASSERT(i < cfg.func_n_as_int);

    // This is all we need so far - the EPs are setup by a later set_interface
    // request (as per UAC2 specification)
    uint16_t drv_len =
        _audiod_fct[i].desc_length -
        TUD_AUDIO_DESC_IAD_LEN;  // - TUD_AUDIO_DESC_IAD_LEN since tinyUSB
                                 // already handles the IAD descriptor

    return drv_len;
  }

  // Handle class co
  bool audiod_control_xfer_cb(uint8_t rhport, uint8_t stage,
                              tusb_control_request_t const *request) {
    if (stage == CONTROL_STAGE_SETUP) {
      return audiod_control_request(rhport, request);
    } else if (stage == CONTROL_STAGE_DATA) {
      return audiod_control_complete(rhport, request);
    }

    return true;
  }

  bool audiod_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result,
                      uint32_t xferred_bytes) {
    (void)result;
    (void)xferred_bytes;

    // Search for interface belonging to given end point address and proceed
    // as required
    for (uint8_t func_id = 0; func_id < cfg.func_n_as_int; func_id++) {
      audiod_function_t *audio = &_audiod_fct[func_id];

      if (cfg.enable_interrupt_ep) {
        // Data transmission of control interrupt finished
        if (audio->ep_int == ep_addr) {
          // According to USB2 specification, maximum payload of interrupt EP
          // is 8 bytes on low speed, 64 bytes on full speed, and 1024 bytes
          // on high speed (but only if an alternate interface other than 0 is
          // used
          // - see specification p. 49) In case there is nothing to send we
          // have to return a NAK - this is taken care of by PHY ??? In case
          // of an erroneous transmission a retransmission is conducted - this
          // is taken care of by PHY ???

          // I assume here, that things above are handled by PHY
          // All transmission is done - what remains to do is to inform job
          // was completed

          if (p_cb) p_cb->int_done_cb(rhport);
          return true;
        }
      }
      if (cfg.is_ep_in()) {
        // Data transmission of audio packet finished
        if (audio->ep_in == ep_addr && audio->alt_setting != 0) {
          // USB 2.0, section 5.6.4, third paragraph, states "An isochronous
          // endpoint must specify its required bus access period. However, an
          // isochronous endpoint must be prepared to handle poll rates faster
          // than the one specified." That paragraph goes on to say "An
          // isochronous IN endpoint must return a zero-length packet whenever
          // data is requested at a faster interval than the specified
          // interval and data is not available." This can only be solved
          // reliably if we load a ZLP after every IN transmission since we
          // can not say if the host requests samples earlier than we
          // declared! Once all samples are collected we overwrite the loaded
          // ZLP.

          // Check if there is data to load into EPs buffer - if not load it
          // with ZLP Be aware - we as a device are not able to know if the
          // host polls for data with a faster rate as we stated this in the
          // descriptors. Therefore we always have to put something into the
          // EPs buffer. However, once we did that, there is no way of
          // aborting this or replacing what we put into the buffer before!
          // This is the only place where we can fill something into the EPs
          // buffer!

          // Load new data
          TU_VERIFY(audiod_tx_done_cb(rhport, audio));

          // Transmission of ZLP is done by audiod_tx_done_cb()
          return true;
        }
      }

      if (cfg.is_ep_out()) {
        // New audio packet received
        if (audio->ep_out == ep_addr) {
          TU_VERIFY(audiod_rx_done_cb(rhport, audio, (uint16_t)xferred_bytes));
          return true;
        }

        if (cfg.enable_feedback_ep) {
          // Transmission of feedback EP finished
          if (audio->ep_fb == ep_addr) {
            if (p_cb) p_cb->fb_done_cb(func_id);

            // Schedule a transmit with the new value if EP is not busy
            if (!usbd_edpt_busy(rhport, audio->ep_fb)) {
              // Schedule next transmission - value is changed
              // bytud_audio_n_fb_set() in the meantime or the old value gets
              // sent
              return audiod_fb_send(rhport, audio);
            }
          }
        }
      }
    }

    return false;
  }

  bool tud_audio_buffer_and_schedule_control_xfer(
      uint8_t rhport, tusb_control_request_t const *p_request, void *data,
      uint16_t len) {
    // Handles only sending of data not receiving
    if (p_request->bmRequestType_bit.direction == TUSB_DIR_OUT) return false;

    // Get corresponding driver index
    uint8_t func_id;
    uint8_t itf = TU_U16_LOW(p_request->wIndex);

    // Conduct checks which depend on the recipient
    switch (p_request->bmRequestType_bit.recipient) {
      case TUSB_REQ_RCPT_INTERFACE: {
        uint8_t entityID = TU_U16_HIGH(p_request->wIndex);

        // Verify if entity is present
        if (entityID != 0) {
          // Find index of audio driver structure and verify entity really
          // exists
          TU_VERIFY(audiod_verify_entity_exists(itf, entityID, &func_id));
        } else {
          // Find index of audio driver structure and verify interface really
          // exists
          TU_VERIFY(audiod_verify_itf_exists(itf, &func_id));
        }
      } break;

      case TUSB_REQ_RCPT_ENDPOINT: {
        uint8_t ep = TU_U16_LOW(p_request->wIndex);

        // Find index of audio driver structure and verify EP really exists
        TU_VERIFY(audiod_verify_ep_exists(ep, &func_id));
      } break;

      // Unknown/Unsupported recipient
      default:
        TU_LOG2("  Unsupported recipient: %d\r\n",
                p_request->bmRequestType_bit.recipient);
        TU_BREAKPOINT();
        return false;
    }

    // Crop length
    if (len > _audiod_fct[func_id].ctrl_buf_sz)
      len = _audiod_fct[func_id].ctrl_buf_sz;

    // Copy into buffer
    TU_VERIFY(0 == tu_memcpy_s(_audiod_fct[func_id].ctrl_buf,
                               _audiod_fct[func_id].ctrl_buf_sz, data,
                               (size_t)len));

    if (cfg.is_ep_in() && cfg.enable_ep_in_flow_control) {
      // Find data for sampling_frequency_control
      if (p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS &&
          p_request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_INTERFACE) {
        uint8_t entityID = TU_U16_HIGH(p_request->wIndex);
        uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
        if (_audiod_fct[func_id].bclock_id_tx == entityID &&
            ctrlSel == AUDIO_CS_CTRL_SAM_FREQ &&
            p_request->bRequest == AUDIO_CS_REQ_CUR) {
          _audiod_fct[func_id].sample_rate_tx =
              tu_unaligned_read32(_audiod_fct[func_id].ctrl_buf);
        }
      }
    }

    // Schedule transmit
    return tud_control_xfer(rhport, p_request,
                            (void *)_audiod_fct[func_id].ctrl_buf, len);
  }

  bool tud_audio_n_fb_set(uint8_t func_id, uint32_t feedback) {
    TU_VERIFY(func_id < cfg.func_n_as_int &&
              _audiod_fct[func_id].p_desc != NULL);

    // Format the feedback value
    if (cfg.enable_feedback_forward_correction) {
      if (TUSB_SPEED_FULL == tud_speed_get()) {
        uint8_t *fb = (uint8_t *)&_audiod_fct[func_id].feedback.value;

        // For FS format is 10.14
        *(fb++) = (feedback >> 2) & 0xFF;
        *(fb++) = (feedback >> 10) & 0xFF;
        *(fb++) = (feedback >> 18) & 0xFF;
        // 4th byte is needed to work correctly with MS Windows
        *fb = 0;
      }
    } else {
      // Send value as-is, caller will choose the appropriate format
      _audiod_fct[func_id].feedback.value = feedback;
    }
    // Schedule a transmit with the new value if EP is not busy - this
    // triggers repetitive scheduling of the feedback value
    if (!usbd_edpt_busy(_audiod_fct[func_id].rhport,
                        _audiod_fct[func_id].ep_fb)) {
      return audiod_fb_send(_audiod_fct[func_id].rhport, &_audiod_fct[func_id]);
    }

    return true;
  }

  void audiod_sof_isr(uint8_t rhport, uint32_t frame_count) {
    (void)rhport;
    (void)frame_count;

    if (cfg.is_ep_out() && cfg.enable_feedback_ep) {
      // Determine feedback value - The feedback method is described
      // in 5.12.4.2 of the USB 2.0 spec Boiled down, the feedback value Ff =
      // n_samples / (micro)frame. Since an accuracy of less than 1 Sample /
      // second is desired, at least n_frames = ceil(2^K * f_s / f_m) frames
      // need to be measured, where K = 10 for full speed and K = 13 for high
      // speed, f_s is the sampling frequency e.g. 48 kHz and f_m is the cpu
      // clock frequency e.g. 100 MHz (or any other master clock whose clock
      // count is available and locked to f_s) The update interval in the
      // (4.10.2.1) Feedback Endpoint Descriptor must be less or equal to 2^(K
      // - P), where P = min( ceil(log2(f_m / f_s)), K) feedback = n_cycles /
      // n_frames * f_s / f_m in 16.16 format, where n_cycles are the number
      // of main clock cycles within fb_n_frames

      // Iterate over audio functions and set feedback value
      for (uint8_t i = 0; i < cfg.func_n_as_int; i++) {
        audiod_function_t *audio = &_audiod_fct[i];

        if (audio->ep_fb != 0) {
          // HS shift need to be adjusted since SOF event is generated for
          // frame only
          uint8_t const hs_adjust =
              (TUSB_SPEED_HIGH == tud_speed_get()) ? 3 : 0;
          uint32_t const interval =
              1UL << (audio->feedback.frame_shift - hs_adjust);
          if (0 == (frame_count & (interval - 1))) {
            if (cfg.enable_feedback_interval_isr && p_cb)
              p_cb->feedback_interval_isr(i, frame_count,
                                          audio->feedback.frame_shift);
          }
        }
      }
    }  // cfg.is_ep_out() && cfg.enable_feedback_ep
  }

  USBAudioConfig &config() { return cfg; }

 protected:
  USBAudioCB *p_cb = nullptr;
  USBAudioConfig cfg;
  // Linear buffer TX in case:
  // - target MCU is not capable of handling a ring buffer FIFO e.g. no
  // hardware buffer is available or driver is would need to be changed
  // dramatically OR
  // - the software encoding is used - in this case the linear buffers serve
  // as a target memory where logical channels are encoded into
  std::vector<CFG_TUD_MEM_SECTION CFG_TUSB_MEM_ALIGN uint8_t>
      lin_buf_in_1;  //[cfg.func_ep_in_size_max];

  std::vector<OUT_SW_BUF_MEM_SECTION CFG_TUSB_MEM_ALIGN uint8_t>
      audio_ep_in_sw_buf_1;  //[cfg.func_ep_in_sw_buffer_size];

  std::vector<OUT_SW_BUF_MEM_SECTION CFG_TUSB_MEM_ALIGN uint8_t>
      audio_ep_out_sw_buf_1;  //[cfg.func_ep_in_sw_buffer_size];

  // Linear buffer RX in case:
  // - target MCU is not capable of handling a ring buffer FIFO e.g. no
  // hardware buffer is available or driver is would need to be changed
  // dramatically OR
  // - the software encoding is used - in this case the linear buffers serve
  // as a target memory where logical channels are encoded into
  std::vector<CFG_TUD_MEM_SECTION CFG_TUSB_MEM_ALIGN uint8_t>
      lin_buf_out_1;  //[FUNC_1_EP_OUT_SZ_MAX];

  // Control buffers
  std::vector<CFG_TUD_MEM_SECTION CFG_TUSB_MEM_ALIGN uint8_t>
      ctrl_buf_1;  //[cfg.func_ctl_buffer_size];

  // Active alternate setting of interfaces
  std::vector<uint8_t> alt_setting_1;  //[cfg.func_n_as_int];

  // buffer for descriptor
  std::vector<uint8_t> descriptor;

  // EP IN software buffers and mutexes
  // TUP_DCD_EDPT_ISO_ALLOC[cfg.func_ep_in_sw_buffer_size];
  osal_mutex_def_t ep_in_ff_mutex_wr_1;  // No need for read mutex as only USB
                                         // driver reads from FIFO

  osal_mutex_def_t ep_out_ff_mutex_rd_1;  // No need for write mutex as only
                                          // USB driver writes into FIFO

  struct audiod_function_t {
    audiod_function_t() {
      memset(this,0, sizeof(audiod_function_t));
    }
    uint8_t n_bytes_per_sample_tx;
    uint8_t n_channels_tx;
    uint8_t format_type_tx = AUDIO_FORMAT_TYPE_I;

    uint8_t rhport;
    uint8_t const
        *p_desc = nullptr;  // Pointer pointing to Standard AC Interface
                  // Descriptor(4.7.1)
                  // - Audio Control descriptor defining audio function

    uint8_t ep_in;      // TX audio data EP.
    uint16_t ep_in_sz;  // Current size of TX EP
    uint8_t
        ep_in_as_intf_num;  // Corresponding Standard AS Interface Descriptor
                            // (4.9.1) belonging to output terminal to which
                            // this EP belongs - 0 is invalid (this fits to
                            // UAC2 specification since AS interfaces can not
                            // have interface number equal to zero)
    uint8_t ep_out;         // Incoming (into uC) audio data EP.
    uint16_t ep_out_sz;     // Current size of RX EP
    uint8_t
        ep_out_as_intf_num;  // Corresponding Standard AS Interface Descriptor
                             // (4.9.1) belonging to input terminal to which
                             // this EP belongs - 0 is invalid (this fits to
                             // UAC2 specification since AS interfaces can not
                             // have interface number equal to zero)

    uint8_t ep_fb;  // Feedback EP.

    uint8_t ep_int;  // Audio control interrupt EP.

    bool mounted;  // Device opened

    /*------------- From this point, data is not cleared by bus reset
     * -------------*/

    uint16_t desc_length;  // Length of audio function descriptor

    struct feedback {
      CFG_TUSB_MEM_ALIGN uint32_t
          value;  // Feedback value for asynchronous mode (in 16.16 format).
      uint32_t
          min_value;  // min value according to UAC2 FMT-2.0 section 2.3.1.1.
      uint32_t
          max_value;  // max value according to UAC2 FMT-2.0 section 2.3.1.1.

      uint8_t
          frame_shift;  // bInterval-1 in unit of frame (FS), micro-frame (HS)
      uint8_t compute_method;

      union {
        uint8_t power_of_2;  // pre-computed power of 2 shift
        float float_const;   // pre-computed float constant

        struct {
          uint32_t sample_freq;
          uint32_t mclk_freq;
        } fixed;

      } compute;

    } feedback;

    // Decoding parameters - parameters are set when alternate AS interface is
    // set by host Coding is currently only supported for EP. Software coding
    // corresponding to AS interfaces without EPs are not supported currently.
    uint32_t sample_rate_tx;
    uint16_t packet_sz_tx[3];
    uint8_t bclock_id_tx;
    uint8_t interval_tx;

    // Encoding parameters - parameters are set when alternate AS interface is
    // set by host

    // Buffer for control requests
    uint8_t *ctrl_buf;
    uint8_t ctrl_buf_sz;

    // Current active alternate settings
    uint8_t *alt_setting;  // We need to save the current alternate setting
                           // this way, because it is possible that there are
                           // AS interfaces which do not have an EP!

    // EP Transfer buffers and FIFOs
    tu_fifo_t ep_out_ff;
    tu_fifo_t ep_in_ff;

    // Audio control interrupt buffer - no FIFO - 6 Bytes according to UAC 2
    // specification (p. 74)
    CFG_TUSB_MEM_ALIGN uint8_t ep_int_buf[6];

    // Linear buffer in case target MCU is not capable of handling a ring
    // buffer FIFO e.g. no hardware buffer is available or driver is would
    // need to be changed dramatically OR the support FIFOs are used
    uint8_t *lin_buf_out;
    uint8_t *lin_buf_in;
  };

  //--------------------------------------------------------------------+
  // INTERNAL OBJECT & FUNCTION DECLARATION
  //--------------------------------------------------------------------+
  std::vector<CFG_TUD_MEM_SECTION audiod_function_t> _audiod_fct;

  // No security checks here - internal function only which should always
  // succeed
  uint8_t audiod_get_audio_fct_idx(audiod_function_t *audio) {
    for (uint8_t cnt = 0; cnt < cfg.func_n_as_int; cnt++) {
      if (&_audiod_fct[cnt] == audio) return cnt;
    }
    return 0;
  }

  inline uint8_t tu_desc_subtype(void const *desc) {
    return ((uint8_t const *)desc)[2];
  }

  bool audiod_rx_done_cb(uint8_t rhport, audiod_function_t *audio,
                         uint16_t n_bytes_received) {
    uint8_t idxItf = 0;
    uint8_t const *dummy2;
    uint8_t idx_audio_fct = 0;

    if (p_cb) {
      idx_audio_fct = audiod_get_audio_fct_idx(audio);
      TU_VERIFY(audiod_get_AS_interface_index(audio->ep_out_as_intf_num, audio,
                                              &idxItf, &dummy2));
    }

    // Call a weak callback here - a possibility for user to get informed an
    // audio packet was received and data gets now loaded into EP FIFO (or
    // decoded into support RX software FIFO)
    if (p_cb) {
      TU_VERIFY(p_cb->rx_done_pre_read_cb(rhport, n_bytes_received,
                                          idx_audio_fct, audio->ep_out,
                                          audio->alt_setting[idxItf]));
    }

    if (cfg.enable_linear_buffer_rx) {
      // Data currently is in linear buffer, copy into EP OUT FIFO
      TU_VERIFY(tu_fifo_write_n(&audio->ep_out_ff, audio->lin_buf_out,
                                n_bytes_received));

      // Schedule for next receive
      TU_VERIFY(usbd_edpt_xfer(rhport, audio->ep_out, audio->lin_buf_out,
                               audio->ep_out_sz),
                false);
    } else {
      // Data is already placed in EP FIFO, schedule for next receive
      TU_VERIFY(usbd_edpt_xfer_fifo(rhport, audio->ep_out, &audio->ep_out_ff,
                                    audio->ep_out_sz),
                false);
    }

    // Call a weak callback here - a possibility for user to get informed
    // decoding was completed
    if (p_cb) {
      TU_VERIFY(p_cb->rx_done_post_read_cb(rhport, n_bytes_received,
                                           idx_audio_fct, audio->ep_out,
                                           audio->alt_setting[idxItf]));
    }

    return true;
  }

  // This function is called once a transmit of an audio packet was
  // successfully completed. Here, we encode samples and place it in IN EP's
  // buffer for next transmission. If you prefer your own (more efficient)
  // implementation suiting your purpose set ENABLE_ENCODING = 0 and use
  // tud_audio_n_write.

  // n_bytes_copied - Informs caller how many bytes were loaded. In case
  // n_bytes_copied = 0, a ZLP is scheduled to inform host no data is
  // available for current frame.

  bool audiod_tx_done_cb(uint8_t rhport, audiod_function_t *audio) {
    uint8_t idxItf;
    uint8_t const *dummy2;

    uint8_t idx_audio_fct = audiod_get_audio_fct_idx(audio);
    TU_VERIFY(audiod_get_AS_interface_index(audio->ep_in_as_intf_num, audio,
                                            &idxItf, &dummy2));

    // Only send something if current alternate interface is not 0 as in this
    // case nothing is to be sent due to UAC2 specifications
    if (audio->alt_setting[idxItf] == 0) return false;

    // Call a weak callback here - a possibility for user to get informed
    // former TX was completed and data gets now loaded into EP in buffer (in
    // case FIFOs are used) or if no FIFOs are used the user may use this call
    // back to load its data into the EP IN buffer by use of
    // tud_audio_n_write_ep_in_buffer().
    if (p_cb)
      TU_VERIFY(p_cb->tx_done_pre_load_cb(rhport, idx_audio_fct, audio->ep_in,
                                          audio->alt_setting[idxItf]));

    // Send everything in ISO EP FIFO
    uint16_t n_bytes_tx;

    // If support FIFOs are used, encode and schedule transmit
    // No support FIFOs, if no linear buffer required schedule transmit, else
    // put data into linear buffer and schedule
    if (cfg.enable_ep_in_flow_control) {
      // packet_sz_tx is based on total packet size, here we want size for
      // each support buffer.
      n_bytes_tx = audiod_tx_packet_size(
          audio->packet_sz_tx, tu_fifo_count(&audio->ep_in_ff),
          audio->ep_in_ff.depth, audio->ep_in_sz);
    } else {
      n_bytes_tx = tu_min16(tu_fifo_count(&audio->ep_in_ff),
                            audio->ep_in_sz);  // Limit up to max packet size,
                                               // more can not be done for ISO
    }
    if (cfg.enable_linear_buffer_tx) {
      tu_fifo_read_n(&audio->ep_in_ff, audio->lin_buf_in, n_bytes_tx);
      TU_VERIFY(
          usbd_edpt_xfer(rhport, audio->ep_in, audio->lin_buf_in, n_bytes_tx));
    } else {
      // Send everything in ISO EP FIFO
      TU_VERIFY(usbd_edpt_xfer_fifo(rhport, audio->ep_in, &audio->ep_in_ff,
                                    n_bytes_tx));
    }

    // Call a weak callback here - a possibility for user to get informed
    // former TX was completed and how many bytes were loaded for the next
    // frame
    if (p_cb)
      TU_VERIFY(p_cb->tx_done_post_load_cb(rhport, n_bytes_tx, idx_audio_fct,
                                           audio->ep_in,
                                           audio->alt_setting[idxItf]));
    return true;
  }

  // This function is called once a transmit of a feedback packet was
  // successfully completed. Here, we get the next feedback value to be sent

  inline bool audiod_fb_send(uint8_t rhport, audiod_function_t *audio) {
    return usbd_edpt_xfer(rhport, audio->ep_fb,
                          (uint8_t *)&audio->feedback.value, 4);
  }

  bool audiod_set_interface(uint8_t rhport,
                            tusb_control_request_t const *p_request) {
    (void)rhport;

    // Here we need to do the following:

    // 1. Find the audio driver assigned to the given interface to be set
    // Since one audio driver interface has to be able to cover an unknown
    // number of interfaces (AC, AS + its alternate settings), the best memory
    // efficient way to solve this is to always search through the
    // descriptors. The audio driver is mapped to an audio function by a
    // reference pointer to the corresponding AC interface of this audio
    // function which serves as a starting point for searching

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

    audiod_function_t *audio = &_audiod_fct[func_id];

    // Look if there is an EP to be closed - for this driver, there are only 3
    // possible EPs which may be closed (only AS related EPs can be closed, AC
    // EP (if present) is always open)
    if (cfg.is_ep_in()) {
      if (audio->ep_in_as_intf_num == itf) {
        audio->ep_in_as_intf_num = 0;
#ifndef TUP_DCD_EDPT_ISO_ALLOC
        usbd_edpt_close(rhport, audio->ep_in);
#endif

        // Clear FIFOs, since data is no longer valid
        tu_fifo_clear(&audio->ep_in_ff);

        // Invoke callback - can be used to stop data sampling
        if (p_cb) TU_VERIFY(p_cb->set_itf_close_EP_cb(rhport, p_request));

        audio->ep_in = 0;  // Necessary?

        if (cfg.enable_ep_in_flow_control) {
          audio->packet_sz_tx[0] = 0;
          audio->packet_sz_tx[1] = 0;
          audio->packet_sz_tx[2] = 0;
        }
      }
    }

    if (cfg.is_ep_out()) {
      if (audio->ep_out_as_intf_num == itf) {
        audio->ep_out_as_intf_num = 0;
#ifndef TUP_DCD_EDPT_ISO_ALLOC
        usbd_edpt_close(rhport, audio->ep_out);
#endif

        // Clear FIFOs, since data is no longer valid
        tu_fifo_clear(&audio->ep_out_ff);
        // Invoke callback - can be used to stop data sampling
        if (p_cb) TU_VERIFY(p_cb->set_itf_close_EP_cb(rhport, p_request));

        audio->ep_out = 0;  // Necessary?

        // Close corresponding feedback EP
        if (cfg.enable_feedback_ep) {
#ifndef TUP_DCD_EDPT_ISO_ALLOC
          usbd_edpt_close(rhport, audio->ep_fb);
#endif
          audio->ep_fb = 0;
          tu_memclr(&audio->feedback, sizeof(audio->feedback));
        }
      }
    }  // cfg.is_ep_out()

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
        uint8_t const *p_desc_parse_for_params = p_desc;
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

            // TODO: We need to set EP non busy since this is not taken care
            // of right now in ep_close() - THIS IS A WORKAROUND!
            usbd_edpt_clear_stall(rhport, ep_addr);

            if (cfg.is_ep_in()) {
              if (tu_edpt_dir(ep_addr) == TUSB_DIR_IN &&
                  desc_ep->bmAttributes.usage ==
                      0x00)  // Check if usage is data EP
              {
                // Save address
                audio->ep_in = ep_addr;
                audio->ep_in_as_intf_num = itf;
                audio->ep_in_sz = tu_edpt_packet_size(desc_ep);

                // If software encoding is enabled, parse for the
                // corresponding parameters - doing this here means only AS
                // interfaces with EPs get scanned for parameters
                if (cfg.enable_ep_in_flow_control)
                  audiod_parse_for_AS_params(audio, p_desc_parse_for_params,
                                             p_desc_end, itf);

                // Reconfigure size of support FIFOs - this is necessary to
                // avoid samples to get split in case of a wrap

                // Schedule first transmit if alternate interface is not zero
                // i.e. streaming is disabled - in case no sample data is
                // available a ZLP is loaded It is necessary to trigger this
                // here since the refill is done with an RX FIFO empty
                // interrupt which can only trigger if something was in there
                TU_VERIFY(audiod_tx_done_cb(rhport, &_audiod_fct[func_id]));
              }
            }  // cfg.is_ep_in()

            if (cfg.is_ep_out()) {
              if (tu_edpt_dir(ep_addr) ==
                  TUSB_DIR_OUT)  // Checking usage not necessary
              {
                // Save address
                audio->ep_out = ep_addr;
                audio->ep_out_as_intf_num = itf;
                audio->ep_out_sz = tu_edpt_packet_size(desc_ep);

                // Prepare for incoming data
                if (cfg.enable_linear_buffer_rx) {
                  TU_VERIFY(
                      usbd_edpt_xfer(rhport, audio->ep_out, audio->lin_buf_out,
                                     audio->ep_out_sz),
                      false);
                } else {
                  TU_VERIFY(
                      usbd_edpt_xfer_fifo(rhport, audio->ep_out,
                                          &audio->ep_out_ff, audio->ep_out_sz),
                      false);
                }
              }

              if (cfg.enable_feedback_ep) {
                if (tu_edpt_dir(ep_addr) == TUSB_DIR_IN &&
                    desc_ep->bmAttributes.usage ==
                        1)  // Check if usage is explicit data feedback
                {
                  audio->ep_fb = ep_addr;
                  audio->feedback.frame_shift = desc_ep->bInterval - 1;

                  // Enable SOF interrupt if callback is implemented
                  if (cfg.enable_feedback_interval_isr)
                    usbd_sof_enable(rhport, SOF_CONSUMER_AUDIO, true);
                }
              }
            }  // cfg.is_ep_out()

            foundEPs += 1;
          }
          p_desc = tu_desc_next(p_desc);
        }

        TU_VERIFY(foundEPs == nEps);

        // Invoke one callback for a final set interface
        if (p_cb) TU_VERIFY(p_cb->set_itf_cb(rhport, p_request));

        if (cfg.enable_feedback_ep) {
          // Prepare feedback computation if callback is available
          if (p_cb) {
            audio_feedback_params_t fb_param;

            p_cb->feedback_params_cb(func_id, alt, &fb_param);
            audio->feedback.compute_method = fb_param.method;

            // Minimal/Maximum value in 16.16 format for full speed (1ms per
            // frame) or high speed (125 us per frame)
            uint32_t const frame_div =
                (TUSB_SPEED_FULL == tud_speed_get()) ? 1000 : 8000;
            audio->feedback.min_value = (fb_param.sample_freq / frame_div - 1)
                                        << 16;
            audio->feedback.max_value = (fb_param.sample_freq / frame_div + 1)
                                        << 16;

            switch (fb_param.method) {
              case AUDIO_FEEDBACK_METHOD_FREQUENCY_FIXED:
              case AUDIO_FEEDBACK_METHOD_FREQUENCY_FLOAT:
              case AUDIO_FEEDBACK_METHOD_FREQUENCY_POWER_OF_2:
                set_fb_params_freq(audio, fb_param.sample_freq,
                                   fb_param.frequency.mclk_freq);
                break;

              // nothing to do
              default:
                break;
            }
          }
        }  // cfg.enable_feedback_ep

        // We are done - abort loop
        break;
      }

      // Moving forward
      p_desc = tu_desc_next(p_desc);
    }

    if (cfg.enable_feedback_ep) {
      // Disable SOF interrupt if no driver has any enabled feedback EP
      bool disable = true;
      for (uint8_t i = 0; i < cfg.func_n_as_int; i++) {
        if (_audiod_fct[i].ep_fb != 0) {
          disable = false;
          break;
        }
      }
      if (disable) usbd_sof_enable(rhport, SOF_CONSUMER_AUDIO, false);
    }

    if (cfg.is_ep_in() && cfg.enable_ep_in_flow_control)
      audiod_calc_tx_packet_sz(audio);

    tud_control_status(rhport, p_request);

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
            if (p_cb) {
              // Check if entity is present and get corresponding driver index
              TU_VERIFY(audiod_verify_entity_exists(itf, entityID, &func_id));

              // Invoke callback
              return p_cb->set_req_entity_cb(rhport, p_request,
                                             _audiod_fct[func_id].ctrl_buf);
            } else {
              TU_LOG2("  No entity set request callback available!\r\n");
              return false;  // In case no callback function is present or
                             // request can not be conducted we stall it
            }
          } else {
            if (p_cb) {
              // Find index of audio driver structure and verify interface
              // really exists
              TU_VERIFY(audiod_verify_itf_exists(itf, &func_id));

              // Invoke callback
              return p_cb->set_req_itf_cb(rhport, p_request,
                                          _audiod_fct[func_id].ctrl_buf);
            } else {
              TU_LOG2("  No interface set request callback available!\r\n");
              return false;  // In case no callback function is present or
                             // request can not be conducted we stall it
            }
          }
        } break;

        case TUSB_REQ_RCPT_ENDPOINT: {
          uint8_t ep = TU_U16_LOW(p_request->wIndex);

          if (p_cb) {
            // Check if entity is present and get corresponding driver index
            TU_VERIFY(audiod_verify_ep_exists(ep, &func_id));

            // Invoke callback
            return p_cb->set_req_ep_cb(rhport, p_request,
                                       _audiod_fct[func_id].ctrl_buf);
          } else {
            TU_LOG2("  No EP set request callback available!\r\n");
            return false;  // In case no callback function is present or
                           // request can not be conducted we stall it
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

  // return false to stall control endpoint (e.g unsupported request)
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

            // In case we got a get request invoke callback - callback needs
            // to answer as defined in UAC2 specification page 89 - 5.
            // Requests
            if (p_request->bmRequestType_bit.direction == TUSB_DIR_IN) {
              if (p_cb) {
                return p_cb->get_req_entity_cb(rhport, p_request);
              } else {
                TU_LOG2("  No entity get request callback available!\r\n");
                return false;  // Stall
              }
            }
          } else {
            // Find index of audio driver structure and verify interface
            // really exists
            TU_VERIFY(audiod_verify_itf_exists(itf, &func_id));

            // In case we got a get request invoke callback - callback needs
            // to answer as defined in UAC2 specification page 89 - 5.
            // Requests
            if (p_request->bmRequestType_bit.direction == TUSB_DIR_IN) {
              if (p_cb) {
                return p_cb->set_itf_cb(rhport, p_request);
              } else {
                TU_LOG2("  No interface get request callback available!\r\n");
                return false;  // Stall
              }
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
            if (p_cb) {
              return p_cb->get_req_ep_cb(rhport, p_request);
            } else {
              TU_LOG2("  No EP get request callback available!\r\n");
              return false;  // Stall
            }
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
                                 _audiod_fct[func_id].ctrl_buf,
                                 _audiod_fct[func_id].ctrl_buf_sz));
      return true;
    }

    // There went something wrong - unsupported control request type
    TU_BREAKPOINT();
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
                               &_audiod_fct[func_id].alt_setting[idxItf], 1));

    TU_LOG2("  Get itf: %u - current alt: %u\r\n", itf,
            _audiod_fct[func_id].alt_setting[idxItf]);

    return true;
  }

  // Invoked when class request DATA stage is finished.
  // return false to stall control EP (e.g Host send non-sense DATA)
  bool audiod_control_completeX(uint8_t rhport,
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
            if (p_cb) {
              // Check if entity is present and get corresponding driver index
              TU_VERIFY(audiod_verify_entity_exists(itf, entityID, &func_id));

              // Invoke callback
              return p_cb->set_req_entity_cb(rhport, p_request,
                                             _audiod_fct[func_id].ctrl_buf);
            } else {
              TU_LOG2("  No entity set request callback available!\r\n");
              return false;  // In case no callback function is present or
                             // request can not be conducted we stall it
            }
          } else {
            if (p_cb) {
              // Find index of audio driver structure and verify interface
              // really exists
              TU_VERIFY(audiod_verify_itf_exists(itf, &func_id));

              // Invoke callback
              return p_cb->set_req_itf_cb(rhport, p_request,
                                          _audiod_fct[func_id].ctrl_buf);
            } else {
              TU_LOG2("  No interface set request callback available!\r\n");
              return false;  // In case no callback function is present or
                             // request can not be conducted we stall it
            }
          }
        } break;

        case TUSB_REQ_RCPT_ENDPOINT: {
          uint8_t ep = TU_U16_LOW(p_request->wIndex);

          if (p_cb) {
            // Check if entity is present and get corresponding driver index
            TU_VERIFY(audiod_verify_ep_exists(ep, &func_id));

            // Invoke callback
            return p_cb->set_req_ep_cb(rhport, p_request,
                                       _audiod_fct[func_id].ctrl_buf);
          } else {
            TU_LOG2("  No EP set request callback available!\r\n");
            return false;  // In case no callback function is present or
                           // request can not be conducted we stall it
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

  bool set_fb_params_freq(audiod_function_t *audio, uint32_t sample_freq,
                          uint32_t mclk_freq) {
    // Check if frame interval is within sane limits
    // The interval value n_frames was taken from the descriptors within
    // audiod_set_interface()

    // n_frames_min is ceil(2^10 * f_s / f_m) for full speed and ceil(2^13 *
    // f_s / f_m) for high speed this lower limit ensures the measures
    // feedback value has sufficient precision
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
          16 - audio->feedback.frame_shift - tu_log2(mclk_freq / sample_freq);
    } else if (audio->feedback.compute_method ==
               AUDIO_FEEDBACK_METHOD_FREQUENCY_FLOAT) {
      audio->feedback.compute.float_const =
          (float)sample_freq / mclk_freq *
          (1UL << (16 - audio->feedback.frame_shift));
    } else {
      audio->feedback.compute.fixed.sample_freq = sample_freq;
      audio->feedback.compute.fixed.mclk_freq = mclk_freq;
    }

    return true;
  }

  uint32_t tud_audio_feedback_update(uint8_t func_id, uint32_t cycles) {
    audiod_function_t *audio = &_audiod_fct[func_id];
    uint32_t feedback;

    switch (audio->feedback.compute_method) {
      case AUDIO_FEEDBACK_METHOD_FREQUENCY_POWER_OF_2:
        feedback = (cycles << audio->feedback.compute.power_of_2);
        break;

      case AUDIO_FEEDBACK_METHOD_FREQUENCY_FLOAT:
        feedback =
            (uint32_t)((float)cycles * audio->feedback.compute.float_const);
        break;

      case AUDIO_FEEDBACK_METHOD_FREQUENCY_FIXED: {
        uint64_t fb64 =
            (((uint64_t)cycles) * audio->feedback.compute.fixed.sample_freq)
            << (16 - audio->feedback.frame_shift);
        feedback = (uint32_t)(fb64 / audio->feedback.compute.fixed.mclk_freq);
      } break;

      default:
        return 0;
    }

    // For Windows:
    // https://docs.microsoft.com/en-us/windows-hardware/drivers/audio/usb-2-0-audio-drivers
    // The size of isochronous packets created by the device must be within
    // the limits specified in FMT-2.0 section 2.3.1.1. This means that the
    // deviation of actual packet size from nominal size must not exceed ±
    // one audio slot (audio slot = channel count samples).
    if (feedback > audio->feedback.max_value)
      feedback = audio->feedback.max_value;
    if (feedback < audio->feedback.min_value)
      feedback = audio->feedback.min_value;

    tud_audio_n_fb_set(func_id, feedback);

    return feedback;
  }

  // This helper function finds for a given audio function and AS interface
  // number the index of the attached driver structure, the index of the
  // interface in the audio function (e.g. the std. AS interface with
  // interface number 15 is the first AS interface for the given audio
  // function and thus gets index zero), and finally a pointer to the std. AS
  // interface, where the pointer always points to the first alternate setting
  // i.e. alternate interface zero.
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

  // This helper function finds for a given AS interface number the index of
  // the attached driver structure, the index of the interface in the audio
  // function (e.g. the std. AS interface with interface number 15 is the
  // first AS interface for the given audio function and thus gets index
  // zero), and finally a pointer to the std. AS interface, where the pointer
  // always points to the first alternate setting i.e. alternate interface
  // zero.
  bool audiod_get_AS_interface_index_global(uint8_t itf, uint8_t *func_id,
                                            uint8_t *idxItf,
                                            uint8_t const **pp_desc_int) {
    // Loop over audio driver interfaces
    uint8_t i;
    for (i = 0; i < cfg.func_n_as_int; i++) {
      if (audiod_get_AS_interface_index(itf, &_audiod_fct[i], idxItf,
                                        pp_desc_int)) {
        *func_id = i;
        return true;
      }
    }

    return false;
  }

  // Verify an entity with the given ID exists and returns also the
  // corresponding driver index
  bool audiod_verify_entity_exists(uint8_t itf, uint8_t entityID,
                                   uint8_t *func_id) {
    uint8_t i;
    for (i = 0; i < cfg.func_n_as_int; i++) {
      // Look for the correct driver by checking if the unique standard AC
      // interface number fits
      if (_audiod_fct[i].p_desc &&
          ((tusb_desc_interface_t const *)_audiod_fct[i].p_desc)
                  ->bInterfaceNumber == itf) {
        // Get pointers after class specific AC descriptors and end of AC
        // descriptors - entities are defined in between
        uint8_t const *p_desc =
            tu_desc_next(_audiod_fct[i].p_desc);  // Points to CS AC descriptor
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

  bool audiod_verify_itf_exists(uint8_t itf, uint8_t *func_id) {
    uint8_t i;
    for (i = 0; i < cfg.func_n_as_int; i++) {
      if (_audiod_fct[i].p_desc) {
        // Get pointer at beginning and end
        uint8_t const *p_desc = _audiod_fct[i].p_desc;
        uint8_t const *p_desc_end = _audiod_fct[i].p_desc +
                                    _audiod_fct[i].desc_length -
                                    TUD_AUDIO_DESC_IAD_LEN;
        // Condition modified from p_desc < p_desc_end to prevent gcc>=12
        // strict-overflow warning
        while (p_desc_end - p_desc > 0) {
          if (tu_desc_type(p_desc) == TUSB_DESC_INTERFACE &&
              ((tusb_desc_interface_t const *)_audiod_fct[i].p_desc)
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

  bool audiod_verify_ep_exists(uint8_t ep, uint8_t *func_id) {
    uint8_t i;
    for (i = 0; i < cfg.func_n_as_int; i++) {
      if (_audiod_fct[i].p_desc) {
        // Get pointer at end
        uint8_t const *p_desc_end =
            _audiod_fct[i].p_desc + _audiod_fct[i].desc_length;

        // Advance past AC descriptors - EP we look for are streaming EPs
        uint8_t const *p_desc = tu_desc_next(_audiod_fct[i].p_desc);
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

  // p_desc points to the AS interface of alternate setting zero
  // itf is the interface number of the corresponding interface - we check if
  // the interface belongs to EP in or EP out to see if it is a TX or RX
  // parameter Currently, only AS interfaces with an EP (in or out) are
  // supposed to be parsed for!
  void audiod_parse_for_AS_params(audiod_function_t *audio,
                                  uint8_t const *p_desc,
                                  uint8_t const *p_desc_end,
                                  uint8_t const as_itf) {
    if (cfg.is_ep_in() && cfg.is_ep_out()) {
      if (as_itf != audio->ep_in_as_intf_num &&
          as_itf != audio->ep_out_as_intf_num)
        return;  // Abort, this interface has no EP, this driver does not
                 // support this currently
    }
    if (cfg.is_ep_in() && !cfg.is_ep_out()) {
      if (as_itf != audio->ep_in_as_intf_num) return;
    }
    if (!cfg.is_ep_in() && cfg.is_ep_out()) {
      if (as_itf != audio->ep_out_as_intf_num) return;
    }

    p_desc = tu_desc_next(p_desc);  // Exclude standard AS interface descriptor
                                    // of current alternate interface descriptor
    // Condition modified from p_desc < p_desc_end to prevent gcc>=12
    // strict-overflow warning
    while (p_desc_end - p_desc > 0) {
      // Abort if follow up descriptor is a new standard interface descriptor
      // - indicates the last AS descriptor was already finished
      if (tu_desc_type(p_desc) == TUSB_DESC_INTERFACE) break;

      // Look for a Class-Specific AS Interface Descriptor(4.9.2) to verify
      // format type and format and also to get number of physical channels
      if (tu_desc_type(p_desc) == TUSB_DESC_CS_INTERFACE &&
          tu_desc_subtype(p_desc) == AUDIO_CS_AS_INTERFACE_AS_GENERAL) {
        if (cfg.is_ep_in()) {
          if (as_itf == audio->ep_in_as_intf_num) {
            audio->n_channels_tx =
                ((audio_desc_cs_as_interface_t const *)p_desc)->bNrChannels;
            audio->format_type_tx =
                (audio_format_type_t)(((audio_desc_cs_as_interface_t const *)
                                           p_desc)
                                          ->bFormatType);
          }
        }

        // Look for a Type I Format Type Descriptor(2.3.1.6 - Audio Formats)
        if (cfg.enable_ep_in_flow_control) {
          if (tu_desc_type(p_desc) == TUSB_DESC_CS_INTERFACE &&
              tu_desc_subtype(p_desc) == AUDIO_CS_AS_INTERFACE_FORMAT_TYPE &&
              ((audio_desc_type_I_format_t const *)p_desc)->bFormatType ==
                  AUDIO_FORMAT_TYPE_I) {
            if (cfg.is_ep_in() && cfg.is_ep_out()) {
              if (as_itf != audio->ep_in_as_intf_num &&
                  as_itf != audio->ep_out_as_intf_num)
                break;  // Abort loop, this interface has no EP, this driver
                        // does not support this currently
            }
            if (cfg.is_ep_in() && !cfg.is_ep_out()) {
              if (as_itf != audio->ep_in_as_intf_num) break;
            }
            if (!cfg.is_ep_in() && cfg.is_ep_out()) {
              if (as_itf != audio->ep_out_as_intf_num) break;
            }

            if (cfg.is_ep_in()) {
              if (as_itf == audio->ep_in_as_intf_num) {
                audio->n_bytes_per_sample_tx =
                    ((audio_desc_type_I_format_t const *)p_desc)->bSubslotSize;
              }
            }
          }
        }
      }
      // Other format types are not supported yet

      p_desc = tu_desc_next(p_desc);
    }
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
      // In the case where nav = INT(nav), ni may vary between INT(nav)-1
      // (small VFP), INT(nav) (medium VFP) and INT(nav)+1 (large VFP).
      audio->packet_sz_tx[0] = packet_sz_tx_min;
      audio->packet_sz_tx[1] = packet_sz_tx_norm;
      audio->packet_sz_tx[2] = packet_sz_tx_max;
    }

    return true;
  }

  uint16_t audiod_tx_packet_size(const uint16_t *norminal_size,
                                 uint16_t data_count, uint16_t fifo_depth,
                                 uint16_t max_depth) {
    // Flow control need a FIFO size of at least 4*Navg
    if (norminal_size[1] && norminal_size[1] <= fifo_depth * 4) {
      // Use blackout to prioritize normal size packet
      int ctrl_blackout = 0;
      uint16_t packet_size;
      uint16_t slot_size = norminal_size[2] - norminal_size[1];
      if (data_count < norminal_size[0]) {
        // If you get here frequently, then your I2S clock deviation is too
        // big !
        packet_size = 0;
      } else if (data_count < fifo_depth / 2 - slot_size && !ctrl_blackout) {
        packet_size = norminal_size[0];
        ctrl_blackout = 10;
      } else if (data_count > fifo_depth / 2 + slot_size && !ctrl_blackout) {
        packet_size = norminal_size[2];
        if (norminal_size[0] == norminal_size[1]) {
          // nav > INT(nav), eg. 44.1k, 88.2k
          ctrl_blackout = 0;
        } else {
          // nav = INT(nav), eg. 48k, 96k
          ctrl_blackout = 10;
        }
      } else {
        packet_size = norminal_size[1];
        if (ctrl_blackout) {
          ctrl_blackout--;
        }
      }
      // Normally this cap is not necessary
      return tu_min16(packet_size, max_depth);
    } else {
      return tu_min16(data_count, max_depth);
    }
  }
};

// TU_ATTR_FAST_FUNC void tud_audio_feedback_interval_isr(uint8_t func_id,
// uint32_t frame_number, uint8_t interval_shift){
//     // call audiod_sof_isr
// }
