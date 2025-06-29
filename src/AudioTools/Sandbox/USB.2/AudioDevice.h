#pragma once

#include <cstdint>
#include <cstring>
#include <functional>

#include "USBAudio2DescriptorBuilder.h"
#include "AudioTools/CoreAudio/AudioTypes.h"
#include "tusb.h"

namespace tinyusb {
// Forward declaration
// class USBAudio2DescriptorBuilder;

// AudioDevice with Audio Class 2.0 and dynamic descriptor support
template <uint8_t ItfNum = 0, uint8_t EPOut = 0x01, uint8_t EPIn = 0x81>
class AudioDevice {
 public:
  /// A.17.7 - Feature Unit Control Selectors
  enum audio_feature_unit_control_selector_t {
    AUDIO_FU_CTRL_UNDEF = 0x00,
    AUDIO_FU_CTRL_MUTE = 0x01,
    AUDIO_FU_CTRL_VOLUME = 0x02,
    AUDIO_FU_CTRL_BASS = 0x03,
    AUDIO_FU_CTRL_MID = 0x04,
    AUDIO_FU_CTRL_TREBLE = 0x05,
    AUDIO_FU_CTRL_GRAPHIC_EQUALIZER = 0x06,
    AUDIO_FU_CTRL_AGC = 0x07,
    AUDIO_FU_CTRL_DELAY = 0x08,
    AUDIO_FU_CTRL_BASS_BOOST = 0x09,
    AUDIO_FU_CTRL_LOUDNESS = 0x0A,
    AUDIO_FU_CTRL_INPUT_GAIN = 0x0B,
    AUDIO_FU_CTRL_GAIN_PAD = 0x0C,
    AUDIO_FU_CTRL_INVERTER = 0x0D,
    AUDIO_FU_CTRL_UNDERFLOW = 0x0E,
    AUDIO_FU_CTRL_OVERVLOW = 0x0F,
    AUDIO_FU_CTRL_LATENCY = 0x10,
  };

 enum audio_feedback_method_t {
  AUDIO_FEEDBACK_METHOD_DISABLED,
  AUDIO_FEEDBACK_METHOD_FREQUENCY_FIXED,
  AUDIO_FEEDBACK_METHOD_FREQUENCY_FLOAT,
  AUDIO_FEEDBACK_METHOD_FREQUENCY_POWER_OF_2, // For driver internal use only
  AUDIO_FEEDBACK_METHOD_FIFO_COUNT
};
 
  using RxCallback =
      std::function<void(AudioDevice&, const uint8_t* data, size_t len)>;
  using TxCallback =
      std::function<void(AudioDevice&, uint8_t* buffer, size_t& len)>;
  using VolumeGetCallback = std::function<int16_t(AudioDevice&)>;
  using VolumeSetCallback = std::function<void(AudioDevice&, int16_t)>;
  using MuteGetCallback = std::function<bool(AudioDevice&)>;
  using MuteSetCallback = std::function<void(AudioDevice&, bool)>;

  static AudioDevice& instance() {
    static AudioDevice inst;
    return inst;
  }

  void setOutput(Print& out) { volumeStream_.setOutput(out); }

  void setInput(Stream& io) {
    volumeStream_.setOutput(io);
    volumeStream_.setStream(io);
  }

  bool begin(AudioInfo info) {
    setAudioInfo(info);
    return begin();
  }

  bool begin() {
    if (muteGetCallback_ == nullptr)
      setMuteGetCallback([](AudioDevice<ItfNum, EPOut, EPIn>& dev) -> bool {
        return dev.current_mute;
      });

    if (muteSetCallback_ == nullptr)
      setMuteSetCallback([](AudioDevice<ItfNum, EPOut, EPIn>& dev, bool m) {
        dev.current_mute = m; /* apply mute */
        if (m) {
          dev.volume_before_mute =
              dev.current_volume; /* store volume before mute */
          dev.current_volume = 0; /* set volume to 0 when muted */
        } else {
          dev.current_volume =
              dev.volume_before_mute; /* restore volume after unmute */
        }
        dev.volumeStream_.setVolume(dev.toFloatVolume(dev.current_volume));
      });

    if (volumeGetCallback_ == nullptr)
      setVolumeGetCallback(
          [](AudioDevice<ItfNum, EPOut, EPIn>& dev) -> int16_t {
            return dev.current_volume;
          });

    if (volumeSetCallback_ == nullptr)
      setVolumeSetCallback(
          [](AudioDevice<ItfNum, EPOut, EPIn>& dev, int16_t v) {
            dev.current_volume = v; /* apply volume */
            dev.volumeStream_.setVolume(dev.toFloatVolume(dev.current_volume));
          });

    if (rxCallback_ == nullptr) {
      setRxCallback([](AudioDevice<ItfNum, EPOut, EPIn>& dev,
                       const uint8_t* data,
                       size_t len) { dev.volumeStream_.write(data, len); });
    }

    if (txCallback_ == nullptr) {
      setTxCallback([](AudioDevice<ItfNum, EPOut, EPIn>& dev, uint8_t* data,
                       size_t len) { dev.volumeStream_.readBytes(data, len); });
    }

    return volumeStream_.begin();
  }

  void setRxCallback(RxCallback cb) { rxCallback_ = cb; }
  void setTxCallback(TxCallback cb) { txCallback_ = cb; }

  void setAudioInfo(AudioInfo info) {
    descriptor_.setSampleRate(info.sample_rate);
    descriptor_.setNumChannels(info.channels);
    descriptor_.setBitsPerSample(info.bits_per_sample);
  }

  // bool send(const uint8_t* data, size_t len) {
  //   if (!tud_ready()) return false;
  //   uint16_t written = endpointWrite(ItfNum, EPIn, data, len);
  //   return written == len;
  // }

  static bool tudAudioTxDoneCb(uint8_t itf, uint8_t ep) {
    if (itf != ItfNum || ep != EPIn) return false;
    auto& inst = instance();
    if (inst.txCallback_) {
      uint16_t packetSize = inst.descriptor_.calcMaxPacketSize();
      uint8_t buffer[packetSize];
      size_t len = packetSize;
      inst.txCallback_(buffer, len);
      endpointWrite(itf, ep, buffer, len);
    }
    return true;
  }

  static bool tudAudioRxDoneCb(uint8_t itf, uint8_t ep) {
    if (itf != ItfNum || ep != EPOut) return false;
    auto& inst = instance();
    if (inst.rxCallback_) {
      uint8_t buffer[128];
      int32_t rlen = endpointRead(itf, ep, buffer, sizeof(buffer));
      if (rlen > 0) inst.rxCallback_(buffer, rlen);
    }
    return true;
  }

  static bool tudAudioSetReqCb(uint8_t rhport,
                               tusb_control_request_t const* req,
                               uint8_t* buffer) {
    return instance().handleAudioClassRequest(req, buffer, true);
  }

  static bool tudAudioGetReqCb(uint8_t rhport,
                               tusb_control_request_t const* req,
                               uint8_t* buffer) {
    return instance().handleAudioClassRequest(req, buffer, false);
  }

  static const uint8_t* tudAudioDescriptorCb(uint8_t itf, uint8_t alt,
                                             uint16_t* len) {
    return instance().descriptor_.buildDescriptor(itf, alt, len);
  }

  /// Get the current volume as a float in the range 0.0 to 1.0
  float volume() const { return toFloatVolume(current_volume); }

 private:
  AudioDevice() = default;
  RxCallback rxCallback_ = nullptr;
  TxCallback txCallback_ = nullptr;
  VolumeGetCallback volumeGetCallback_ = nullptr;
  VolumeSetCallback volumeSetCallback_ = nullptr;
  MuteGetCallback muteGetCallback_ = nullptr;
  MuteSetCallback muteSetCallback_ = nullptr;
  bool current_mute = false;
  int16_t current_volume = 0;
  int16_t volume_before_mute = 0;
  VolumeStream volumeStream_;
  USBAudio2DescriptorBuilder descriptor_{EPIn, EPOut};
  struct audiod_function_t {
    audiod_function_t() { memset(this, 0, sizeof(audiod_function_t)); }
    uint8_t n_bytes_per_sample_tx;
    uint8_t n_channels_tx;
    uint8_t format_type_tx = AUDIO_FORMAT_TYPE_I;

    uint8_t rhport;
    uint8_t const* p_desc =
        nullptr;  // Pointer pointing to Standard AC Interface
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
    uint8_t* ctrl_buf;
    uint8_t ctrl_buf_sz;

    // Current active alternate settings
    uint8_t* alt_setting;  // We need to save the current alternate setting
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
    uint8_t* lin_buf_out;
    uint8_t* lin_buf_in;
  };

  static std::vector<CFG_TUD_MEM_SECTION audiod_function_t> _audiod_fct;

  /// map Range: –32 768 to +32 767 to 0.0 to 1.0
  float toFloatVolume(int16_t intVol) const {
    return (32768.0f + intVol) / 65536.0f;
  }

  static inline uint16_t endpointWrite(uint8_t itf, uint8_t ep,
                                       const void* buffer, uint16_t size) {
    (void)itf;
    return 0;
  }

  static inline int32_t endpointRead(uint8_t itf, uint8_t ep, void* buffer,
                                     uint16_t size) {
    (void)itf;
    return 0;
  }

  void setVolumeGetCallback(VolumeGetCallback cb) { volumeGetCallback_ = cb; }

  void setVolumeSetCallback(VolumeSetCallback cb) { volumeSetCallback_ = cb; }

  void setMuteGetCallback(MuteGetCallback cb) { muteGetCallback_ = cb; }

  void setMuteSetCallback(MuteSetCallback cb) { muteSetCallback_ = cb; }

  bool handleAudioClassRequest(tusb_control_request_t const* req,
                               uint8_t* buffer, bool isSet) {
    const uint8_t cs = (req->wValue >> 8) & 0xFF;
    const uint8_t entityId = (req->wIndex >> 8) & 0xFF;
    if (entityId != 2) return false;
    switch (req->bRequest) {
      case AUDIO_CS_REQ_CUR:
        if (cs == AUDIO_FU_CTRL_MUTE) {
          if (isSet) {
            bool mute = buffer[0];
            if (muteSetCallback_) muteSetCallback_(*this, mute);
          } else {
            buffer[0] = muteGetCallback_ ? muteGetCallback_(*this) : 0;
          }
          return true;
        } else if (cs == AUDIO_FU_CTRL_VOLUME) {
          if (isSet) {
            int16_t vol = buffer[0] | (buffer[1] << 8);
            if (volumeSetCallback_) volumeSetCallback_(*this, vol);
          } else {
            int16_t vol = volumeGetCallback_ ? volumeGetCallback_(*this) : 0;
            buffer[0] = vol & 0xFF;
            buffer[1] = (vol >> 8) & 0xFF;
          }
          return true;
        }
        break;
    }
    return false;
  }

  //--------------------------------------------------------------------+
  // READ API
  //--------------------------------------------------------------------+

  static uint16_t tud_audio_n_available(uint8_t func_id) {
    TU_VERIFY(func_id < CFG_TUD_AUDIO && _audiod_fct[func_id].p_desc != NULL);
    return tu_fifo_count(&_audiod_fct[func_id].ep_out_ff);
  }

  static uint16_t tud_audio_n_read(uint8_t func_id, void* buffer,
                                   uint16_t bufsize) {
    TU_VERIFY(func_id < CFG_TUD_AUDIO && _audiod_fct[func_id].p_desc != NULL);
    return tu_fifo_read_n(&_audiod_fct[func_id].ep_out_ff, buffer, bufsize);
  }

  static bool tud_audio_n_clear_ep_out_ff(uint8_t func_id) {
    TU_VERIFY(func_id < CFG_TUD_AUDIO && _audiod_fct[func_id].p_desc != NULL);
    return tu_fifo_clear(&_audiod_fct[func_id].ep_out_ff);
  }

  static tu_fifo_t* tud_audio_n_get_ep_out_ff(uint8_t func_id) {
    if (func_id < CFG_TUD_AUDIO && _audiod_fct[func_id].p_desc != NULL)
      return &_audiod_fct[func_id].ep_out_ff;
    return NULL;
  }

  // This function is called once an audio packet is received by the USB and is
  // responsible for putting data from USB memory into EP_OUT_FIFO.
  static bool audiod_rx_done_cb(uint8_t rhport, audiod_function_t* audio,
                                uint16_t n_bytes_received) {
    uint8_t idxItf = 0;
    uint8_t const* dummy2;
    uint8_t idx_audio_fct = 0;

    idx_audio_fct = audiod_get_audio_fct_idx(audio);
    TU_VERIFY(audiod_get_AS_interface_index(audio->ep_out_as_intf_num, audio,
                                            &idxItf, &dummy2));

    // Call a weak callback here - a possibility for user to get informed an
    // audio packet was received and data gets now loaded into EP FIFO
    TU_VERIFY(tud_audio_rx_done_pre_read_cb(rhport, n_bytes_received,
                                            idx_audio_fct, audio->ep_out,
                                            audio->alt_setting[idxItf]));

    // Data currently is in linear buffer, copy into EP OUT FIFO
    TU_VERIFY(tu_fifo_write_n(&audio->ep_out_ff, audio->lin_buf_out,
                              n_bytes_received));

    // Schedule for next receive
    TU_VERIFY(usbd_edpt_xfer(rhport, audio->ep_out, audio->lin_buf_out,
                             audio->ep_out_sz),
              false);

    if (audio->feedback.compute_method == AUDIO_FEEDBACK_METHOD_FIFO_COUNT) {
      audiod_fb_fifo_count_update(audio, tu_fifo_count(&audio->ep_out_ff));
    }

    // Call a weak callback here - a possibility for user to get informed
    // decoding was completed
    TU_VERIFY(tud_audio_rx_done_post_read_cb(rhport, n_bytes_received,
                                             idx_audio_fct, audio->ep_out,
                                             audio->alt_setting[idxItf]));

    return true;
  }

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
  static uint16_t tud_audio_n_write(uint8_t func_id, const void* data,
                                    uint16_t len) {
    TU_VERIFY(func_id < CFG_TUD_AUDIO && _audiod_fct[func_id].p_desc != NULL);
    return tu_fifo_write_n(&_audiod_fct[func_id].ep_in_ff, data, len);
  }

  static bool tud_audio_n_clear_ep_in_ff(
      uint8_t func_id)  // Delete all content in the EP IN FIFO
  {
    TU_VERIFY(func_id < CFG_TUD_AUDIO && _audiod_fct[func_id].p_desc != NULL);
    return tu_fifo_clear(&_audiod_fct[func_id].ep_in_ff);
  }

  static tu_fifo_t* tud_audio_n_get_ep_in_ff(uint8_t func_id) {
    if (func_id < CFG_TUD_AUDIO && _audiod_fct[func_id].p_desc != NULL)
      return &_audiod_fct[func_id].ep_in_ff;
    return NULL;
  }

  // This function is called once a transmit of an audio packet was successfully
  // completed. Here, we encode samples and place it in IN EP's buffer for next
  // transmission. n_bytes_copied - Informs caller how many bytes were loaded.
  // In case n_bytes_copied = 0, a ZLP is scheduled to inform host no data is
  // available for current frame.
  static bool audiod_tx_done_cb(uint8_t rhport, audiod_function_t* audio) {
    uint8_t idxItf;
    uint8_t const* dummy2;

    uint8_t idx_audio_fct = audiod_get_audio_fct_idx(audio);
    TU_VERIFY(audiod_get_AS_interface_index(audio->ep_in_as_intf_num, audio,
                                            &idxItf, &dummy2));

    // Only send something if current alternate interface is not 0 as in this
    // case nothing is to be sent due to UAC2 specifications
    if (audio->alt_setting[idxItf] == 0) return false;

    // Call a weak callback here - a possibility for user to get informed former
    // TX was completed and data gets now loaded into EP in buffer (in case
    // FIFOs are used) or if no FIFOs are used the user may use this call back
    // to load its data into the EP IN buffer by use of
    // tud_audio_n_write_ep_in_buffer().
    TU_VERIFY(tud_audio_tx_done_pre_load_cb(rhport, idx_audio_fct, audio->ep_in,
                                            audio->alt_setting[idxItf]));

    // Send everything in ISO EP FIFO
    uint16_t n_bytes_tx;

    // packet_sz_tx is based on total packet size
    n_bytes_tx = audiod_tx_packet_size(audio->packet_sz_tx,
                                       tu_fifo_count(&audio->ep_in_ff),
                                       audio->ep_in_ff.depth, audio->ep_in_sz);
    tu_fifo_read_n(&audio->ep_in_ff, audio->lin_buf_in, n_bytes_tx);
    TU_VERIFY(
        usbd_edpt_xfer(rhport, audio->ep_in, audio->lin_buf_in, n_bytes_tx));

    // Call a weak callback here - a possibility for user to get informed former
    // TX was completed and how many bytes were loaded for the next frame
    TU_VERIFY(tud_audio_tx_done_post_load_cb(rhport, n_bytes_tx, idx_audio_fct,
                                             audio->ep_in,
                                             audio->alt_setting[idxItf]));

    return true;
  }
};

}  // namespace tinyusb
