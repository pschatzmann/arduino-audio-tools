#pragma once

#include <vector>

#include "Arduino.h"
#include "USBDeviceAudioAPI.h"
#include "common/tusb_types.h"

//--------------------------------------------------------------------
// Debugging Logging and Testing
//--------------------------------------------------------------------
#define AUDIO_LOG(...)               \
  {                                  \
    char msg[160];                   \
    snprintf(msg, 160, __VA_ARGS__); \
    LOG_AUDIO_OUTPUT.println(msg);   \
    LOG_AUDIO_OUTPUT.flush();        \
  }
#define AUDIO_NO_LOG(...)

#define LOG_AUDIO_OUTPUT Serial
#define AUDIO_DEBUG false
#define LOG_AUDIO_ERROR AUDIO_LOG
#define LOG_AUDIO_DEBUG AUDIO_LOG

#ifdef ESP32
#define ISO_FB_EP 3
#else
#define ISO_FB_EP 3
#endif

//--------------------------------------------------------------------
// Unit numbers are arbitrary selected
//--------------------------------------------------------------------

#define UAC2_ENTITY_CLOCK 0x10

// Speaker path
#define UAC2_ENTITY_SPK_INPUT_TERMINAL 0x15
#define UAC2_ENTITY_SPK_FEATURE_UNIT 0x16
#define UAC2_ENTITY_SPK_OUTPUT_TERMINAL 0x17

// Microphone path
#define UAC2_ENTITY_MIC_INPUT_TERMINAL 0x11
#define UAC2_ENTITY_MIC_FEATURE_UNIT 0x12
#define UAC2_ENTITY_MIC_OUTPUT_TERMINAL 0x13

// output using 5 debug pins
#if AUDIO_DEBUG
#define debugWrite(pin, active) digitalWrite(pin, active);
#else
#define debugWrite(pin, active)
#endif

/// Status management e.g. for LED delays
enum class AudioProcessingStatus {
  INACTIVE = 0,
  ERROR = 500,
  PLAYING = 1000,
  ACTIVE = 2000
};

/// A resizable buffer of bytes which manages the available bytes
class ByteBuffer {
 public:
  // resizes the buffer
  void resize(int size) { vector.resize(size); }
  // provides access to the first byte of the data
  uint8_t *data() { return vector.data(); }
  // remove first n bytes
  void consume(int n) {
    pos -= n;
    if (pos > 0) {
      memmove(data(), data() + n, pos);
    }
  }
  // provides the available byte count
  int available() { return pos; }
  // provides the size of the buffer
  int size() { return vector.size(); }
  // sets the available byte count to 0
  void reset() { int pos = 0; }
  // clears the memory by setting the content to 0
  void clear() { memset(data(), 0, size()); }
  // inform about number of available bytes
  void setAvailable(int av) { pos = av; }

 protected:
  std::vector<uint8_t> vector;
  int pos = 0;
};

/***
 * USB Audio Device:
 * - provide data access via callbacks
 * - configure audio info via begin method
 * - provide all potential methods so that we can easily overwrite them
 * - implement Speaker (device is audio sink)
 * - implement Microphone (device is audio source)
 * - dont change the audio based on mute or volume changes: this is the
 * respondibility of the implementor
 */

class USBDeviceAudio : public USBAudioCB {
 public:
  //USBDeviceAudio() { setupDebugPins(); }
  USBDeviceAudio() = default;

  /// callback for audio sink (speaker): we write out the received data e.g. to
  /// a dac
  void setWriteCallback(size_t (*write_cb)(const uint8_t *data, size_t len,
                                           USBDeviceAudio &ref)) {
    cfg.p_write_callback = write_cb;
  }

  /// callback for audio source (microphone): we provide the audio data e.g.
  /// from the adc
  void setReadCallback(size_t (*read_cb)(uint8_t *data, size_t len,
                                         USBDeviceAudio &ref)) {
    cfg.p_read_callback = read_cb;
  }

  /// Alternative to setWriteCallback
  void setOutput(Print &out) {
    p_print = &out;
    setWriteCallback(defaultWriteCB);
  }

  /// Alternaive to setReadCallback
  void setInput(Stream &in) {
    p_stream = &in;
    setReadCallback(defaultReadCB);
  }

  USBAudioConfig defaultConfig() {
    USBAudioConfig result;
    return result;
  }

  /*------------- MAIN -------------*/
  virtual bool begin(USBAudioConfig config) {
    _itf_number_total = 0;
    cfg = config;

    setupDebugPins();

    _api.begin(this, cfg);
    _mute.resize(cfg.channels + 1);
    _volume.resize(cfg.channels + 1);

    // check data
    if (!isMicrophone() && !isSpeaker()) {
      LOG_AUDIO_ERROR("No callback has been defined");
      setStatus(AudioProcessingStatus::ERROR);
      return false;
    }

    // calculate descriptor length;
    if (interfaceDescriptor(nullptr, 1024) == 0) {
      setStatus(AudioProcessingStatus::ERROR);
      LOG_AUDIO_ERROR("Interface Descriptor length was 0");
      return false;
    }

    _clk_is_valid = 1;
    setStatus(AudioProcessingStatus::ACTIVE);

    return true;
  }

  void end() {
    tud_deinit(cfg.rh_port);
    setStatus(AudioProcessingStatus::INACTIVE);
    if (_out_buffer.size() > 0) _out_buffer.resize(0);
    if (_in_buffer.size() > 0) _out_buffer.resize(0);
  }

  // If is mounted
  bool active(void) {
    return status() == AudioProcessingStatus::ACTIVE ||
           status() == AudioProcessingStatus::PLAYING;
  }

  operator bool() { return active(); }

  // get sample rate
  uint32_t rate() { return cfg.sample_rate; }

  // get number of channels
  int channels() { return cfg.channels; }

  // provides the volume for the indicated channel (from 1 to 100)
  uint16_t volume(int channel) { return _volume[channel]; }

  // checks if the channel is muted
  bool isMute(int channel) { return _mute[channel]; }

  /// Call from loop to blink led
  bool updateLED(int pin) {
    if (_is_led_setup) {
      pinMode(pin, OUTPUT);
      _is_led_setup = false;
    }

    // led must be active
    if (_processing_status != AudioProcessingStatus::INACTIVE &&
        millis() > _led_timeout) {
      _led_timeout = millis() + (uint16_t)_processing_status;
      _led_active = !_led_active;
      digitalWrite(pin, _led_active);
      return true;
    }

    // led is inactive
    if (_processing_status == AudioProcessingStatus::INACTIVE) {
      if (_led_active) {
        _led_active = false;
        digitalWrite(pin, _led_active);
      }
    }
    return false;
  }

  /// Provide the actual status
  AudioProcessingStatus status() { return _processing_status; }

  bool isMicrophone() {
    return (cfg.p_read_callback != nullptr && cfg.p_read_callback != defaultReadCB) ||
           (cfg.p_read_callback == defaultReadCB && p_stream != nullptr);
  }

  bool isSpeaker() {
    return (cfg.p_write_callback != nullptr &&
            cfg.p_write_callback != defaultWriteCB) ||
           (cfg.p_write_callback == defaultWriteCB && p_print != nullptr);
  }

  bool isHeadset() { return isSpeaker() && isMicrophone(); }

  // USBD_Device device() { return TinyUSBDevice; }

  //--------------------------------------------------------------------+
  // Application Callback API Implementations
  //--------------------------------------------------------------------+
  // from USBD_Interface

  virtual uint16_t getMaxEPSize() {
    return TUD_AUDIO_EP_SIZE(cfg.sample_rate, cfg.bits_per_sample / 8,
                             cfg.channels);
  }

  virtual uint16_t getIOSize() {
    return TUD_AUDIO_EP_SIZE(cfg.sample_rate, cfg.bits_per_sample / 8,
                             cfg.channels);
  }

  virtual uint8_t getFeatureUnitLength() { return (6 + (channels() + 1) * 4); }

  // Invoked when set interface is called, typically on start/stop streaming or
  // format change
  bool set_itf_cb(uint8_t rhport,
                  tusb_control_request_t const *p_request) override {
    uint8_t const itf = tu_u16_low(tu_le16toh(p_request->wIndex));
    uint8_t const alt = tu_u16_low(tu_le16toh(p_request->wValue));

    if (alt != 0) setStatus(AudioProcessingStatus::PLAYING);

    return true;
  }

  // Invoked when audio class specific set request received for an EP
  bool set_req_ep_cb(uint8_t rhport, tusb_control_request_t const *p_request,
                     uint8_t *pBuff) override {
    (void)rhport;
    (void)pBuff;

    // We do not support any set range requests here, only current value
    // requests
    TU_VERIFY(p_request->bRequest == AUDIO_CS_REQ_CUR);

    // Page 91 in UAC2 specification
    uint8_t channelNum = TU_U16_LOW(p_request->wValue);
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    uint8_t ep = TU_U16_LOW(p_request->wIndex);

    (void)channelNum;
    (void)ctrlSel;
    (void)ep;

    return false;  // Yet not implemented
  }

  // Invoked when audio class specific set request received for an interface
  bool set_req_itf_cb(uint8_t rhport, tusb_control_request_t const *p_request,
                      uint8_t *pBuff) override {
    (void)rhport;
    (void)pBuff;

    // We do not support any set range requests here, only current value
    // requests
    TU_VERIFY(p_request->bRequest == AUDIO_CS_REQ_CUR);

    // Page 91 in UAC2 specification
    uint8_t channelNum = TU_U16_LOW(p_request->wValue);
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    uint8_t itf = TU_U16_LOW(p_request->wIndex);

    (void)channelNum;
    (void)ctrlSel;
    (void)itf;

    return false;  // Yet not implemented
  }

  // Invoked when audio class specific set request received for an entity
  bool set_req_entity_cb(uint8_t rhport,
                         tusb_control_request_t const *p_request,
                         uint8_t *buf) override {
    // Page 91 in UAC2 specification
    uint8_t channelNum = TU_U16_LOW(p_request->wValue);
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    uint8_t itf = TU_U16_LOW(p_request->wIndex);
    uint8_t entityID = TU_U16_HIGH(p_request->wIndex);
    audio_control_request_t const *request =
        (audio_control_request_t const *)p_request;

    debugWrite(5, HIGH);

    // for speaker
    if (request->bEntityID == UAC2_ENTITY_SPK_FEATURE_UNIT) {
      bool rc = feature_unit_set_request(rhport, p_request, buf);
      debugWrite(5, LOW);
      return rc;
    }
    if (request->bEntityID == UAC2_ENTITY_CLOCK) {
      bool rc = clock_set_request(rhport, p_request, buf);
      debugWrite(5, LOW);
      return rc;
    }

    debugWrite(5, HIGH);
    LOG_AUDIO_DEBUG(
        "Set request not handled, entity = %d, selector = %d, request = %d",
        request->bEntityID, request->bControlSelector, request->bRequest);
    return false;  // Yet not implemented
  }

  // Invoked when audio class specific get request received for an EP
  bool get_req_ep_cb(uint8_t rhport,
                     tusb_control_request_t const *p_request) override {
    (void)rhport;

    // Page 91 in UAC2 specification
    uint8_t channelNum = TU_U16_LOW(p_request->wValue);
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    uint8_t ep = TU_U16_LOW(p_request->wIndex);

    (void)channelNum;
    (void)ctrlSel;
    (void)ep;

    //	return tud_control_xfer(rhport, p_request, &tmp, 1);

    return false;  // Yet not implemented
  }

  // Invoked when audio class specific get request received for an interface
  bool get_req_itf_cb(uint8_t rhport,
                      tusb_control_request_t const *p_request) override {
    (void)rhport;

    // Page 91 in UAC2 specification
    uint8_t channelNum = TU_U16_LOW(p_request->wValue);
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    uint8_t itf = TU_U16_LOW(p_request->wIndex);

    (void)channelNum;
    (void)ctrlSel;
    (void)itf;

    return false;  // Yet not implemented
  }

  // Invoked when audio class specific get request received for an entity
  bool get_req_entity_cb(uint8_t rhport,
                         tusb_control_request_t const *p_request) override {
    cfg.rh_port = rhport;
    // Page 91 in UAC2 specification
    uint8_t channelNum = TU_U16_LOW(p_request->wValue);
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    // uint8_t itf = TU_U16_LOW(p_request->wIndex); 			// Since
    // we have only one audio function implemented, we do not need the itf value
    uint8_t entityID = TU_U16_HIGH(p_request->wIndex);
    audio_control_request_t const *request =
        (audio_control_request_t const *)p_request;

    debugWrite(6, HIGH);

    // Clock Source
    if (request->bEntityID == UAC2_ENTITY_CLOCK) {
      bool rc = clock_get_request(rhport, p_request);
      if (rc) debugWrite(6, LOW);
      return rc;
    }

    // Fueature unit speaker
    if (request->bEntityID == UAC2_ENTITY_SPK_FEATURE_UNIT) {
      bool rc = feature_unit_get_request(rhport, p_request);
      if (rc) debugWrite(6, LOW);
      return rc;
    }

    // Feature unit mic
    if (entityID == UAC2_ENTITY_MIC_FEATURE_UNIT) {
      bool rc = feature_unit_get_request(rhport, p_request);
      if (rc) debugWrite(6, LOW);
      return rc;
    }

    // Input terminal (Microphone input)
    if (entityID == UAC2_ENTITY_MIC_INPUT_TERMINAL ||
        entityID == UAC2_ENTITY_SPK_INPUT_TERMINAL) {
      switch (ctrlSel) {
        case AUDIO_TE_CTRL_CONNECTOR: {
          // The terminal connector control only has a get request with only the
          // CUR attribute.
          audio_desc_channel_cluster_t ret;
          ret.bNrChannels = cfg.channels;
          ret.bmChannelConfig = (audio_channel_config_t)0;
          ret.iChannelNames = 0;

          LOG_AUDIO_DEBUG("    Get terminal connector");

          bool rc = _api.tud_audio_buffer_and_schedule_control_xfer(
              rhport, p_request, (void *)&ret, sizeof(ret));
          if (rc) debugWrite(6, LOW);
          return rc;
        } break;

          // Unknown/Unsupported control selector
        default:
          LOG_AUDIO_DEBUG("  Unsupported selector: %d", entityID);
          debugWrite(6, HIGH);
          return false;
      }
    }

    LOG_AUDIO_DEBUG("  Unsupported entity: %d", entityID);
    debugWrite(6, HIGH);
    return false;  // Yet not implemented
  }

  bool tx_done_pre_load_cb(uint8_t rhport, uint8_t itf, uint8_t ep_in,
                           uint8_t cur_alt_setting) override {
    // (void)rhport;
    // (void)itf;
    // (void)ep_in;
    // (void)cur_alt_setting;

    // fill buffer from "microphone" input
    if (isMicrophone()) {
      debugWrite(1, HIGH);
      // manage buffer size
      uint16_t len = getIOSize() - 2;  // CFG_TUD_AUDIO_EP_SZ_IN - 2;
      if (_out_buffer.size() < len) _out_buffer.resize(len);

      // return if the buffer is already filled
      if (_out_buffer.available() != 0) {
        return true;
      }

      // fill the buffer with data
      uint8_t *adr = _out_buffer.data();
      _out_buffer.clear();
      int len_eff = (*cfg.p_read_callback)(adr, len, *this);
      _out_buffer.setAvailable(len_eff);
      debugWrite(1, LOW);
    }

    return true;
  }

  bool tx_done_post_load_cb(uint8_t rhport, uint16_t n_bytes_copied,
                            uint8_t itf, uint8_t ep_in,
                            uint8_t cur_alt_setting) override {
    (void)rhport;
    (void)itf;
    (void)ep_in;
    (void)cur_alt_setting;

    // output audio from "microphone" buffer to usb
    if (isMicrophone()) {
      debugWrite(2, HIGH);
      uint8_t *adr = _out_buffer.data();
      _api.tud_audio_n_write(func_id, adr, _out_buffer.available());
      _out_buffer.reset();
      debugWrite(2, LOW);
    }

    return true;
  }

  bool rx_done_pre_read_cb(uint8_t rhport, uint16_t n_bytes_received,
                           uint8_t func_id, uint8_t ep_out,
                           uint8_t cur_alt_setting) override {
    // read audio from usb
    if (isSpeaker() && _in_buffer.available() == 0) {
      debugWrite(3, HIGH);
      uint16_t len = _api.tud_audio_n_available(func_id);
      if (len > 0) {
        if (_in_buffer.size() < len) _in_buffer.resize(len);
        uint8_t *adr = _in_buffer.data();
        int len_eff = _api.tud_audio_n_read(func_id, adr, len);
        _in_buffer.setAvailable(len_eff);
      }
      debugWrite(3, LOW);
      return true;
    }
    return true;
  }

  bool rx_done_post_read_cb(uint8_t rhport, uint16_t n_bytes_received,
                            uint8_t func_id, uint8_t ep_out,
                            uint8_t cur_alt_setting) override {
    // read audio from usb
    if (isSpeaker() && _in_buffer.available() > 0) {
      debugWrite(4, HIGH);
      uint8_t *adr = _in_buffer.data();
      size_t rc = cfg.p_write_callback(adr, _in_buffer.available(), *this);
      _in_buffer.consume(rc);
      debugWrite(4, LOW);
    }
    return true;
  }

  bool set_itf_close_EP_cb(uint8_t rhport,
                           tusb_control_request_t const *p_request) override {
    (void)rhport;
    uint8_t const itf = tu_u16_low(tu_le16toh(p_request->wIndex));
    uint8_t const alt = tu_u16_low(tu_le16toh(p_request->wValue));
    if (alt == 0) setStatus(AudioProcessingStatus::ACTIVE);

    return true;
  }
  // for speaker
  void feedback_params_cb(uint8_t func_id, uint8_t alt_itf,
                          audio_feedback_params_t *feedback_param) override {
    (void)func_id;
    (void)alt_itf;
    // Set feedback method to fifo counting
    feedback_param->method = AUDIO_FEEDBACK_METHOD_FIFO_COUNT;
    feedback_param->sample_freq = cfg.sample_rate;
  }

  /// Determine the interface descriptor length
  size_t getInterfaceDescriptorLength(uint8_t itfnum) override {
    return getInterfaceDescriptor(nullptr, 0);
  }

  // build interface descriptor
  uint16_t getInterfaceDescriptor(uint8_t *buf,
                                  uint16_t bufsize)  {
    // if no source or sink then audio is not active
    if (!isMicrophone() && !isSpeaker()) {
      return 0;
    }

    // if we know the len, we can return it
    if (buf == nullptr && _desc_len > 0) {
      return _desc_len;
    }

    // the buffer was too small
    if (_desc_len > 0 && bufsize < _desc_len) {
      return 0;
    }

    return interfaceDescriptor(buf, bufsize);
  }

  inline USBDeviceAudioAPI& api() { return _api; }

 protected:
  bool _is_led_setup = true;
  AudioProcessingStatus _processing_status = AudioProcessingStatus::INACTIVE;
  std::vector<bool> _mute;        // +1 for master channel 0
  std::vector<uint16_t> _volume;  // +1 for master channel 0
  uint8_t _clk_is_valid = true;
  ByteBuffer _in_buffer;
  ByteBuffer _out_buffer;
  bool _led_active = false;
  uint64_t _led_timeout = 0;

  // persisted descriptor data
  uint8_t _itfnum_spk = 0;
  uint8_t _itfnum_mic = 0;
  uint8_t _itf_number_total = 0;
  uint8_t _itfnum_ctl = 0;
  uint8_t _ep_ctl = 0;
  uint8_t _ep_mic = 0;
  uint8_t _ep_spk = 0;
  uint8_t _ep_fb = 0;
  uint8_t _ep_int = 0;
  uint8_t _stridx = 0;
  int _desc_append_pos = 0;
  int _desc_len = 0;

  // input/output callbacks
  Stream *p_stream = nullptr;
  Print *p_print = nullptr;
  USBDeviceAudioAPI _api;
  USBAudioConfig cfg;
  std::vector<uint8_t> interface_descriptor;

  /// Define the led delay
  void setStatus(AudioProcessingStatus status) { _processing_status = status; }

  /// We can use 8 debug pins with a logic analyser
  /// We can use 8 debug pins with a logic analyser
  void setupDebugPins() {
#if AUDIO_DEBUG
    for (int j = 0; j < 8; j++) {
      pinMode(j, OUTPUT);
    }
#endif
  }

  void append(uint8_t *to, uint8_t *str, int len) {
    if (to != nullptr) memcpy(to + _desc_append_pos, str, len);
    _desc_append_pos += len;
  }

  static size_t defaultWriteCB(const uint8_t *data, size_t len,
                               USBDeviceAudio &ref) {
    Print *p_print = ref.p_print;
    if (p_print) return p_print->write((const uint8_t *)data, len);
    return 0;
  }

  static size_t defaultReadCB(uint8_t *data, size_t len, USBDeviceAudio &ref) {
    Stream *p_stream = ref.p_stream;
    if (p_stream) return p_stream->readBytes((uint8_t *)data, len);
    return 0;
  }

  // Helper methods
  virtual bool feature_unit_get_request(
      uint8_t rhport, tusb_control_request_t const *p_request) {
    // Page 91 in UAC2 specification
    uint8_t channelNum = TU_U16_LOW(p_request->wValue);
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    // uint8_t itf = TU_U16_LOW(p_request->wIndex); 			// Since
    // we have only one audio function implemented, we do not need the itf value
    uint8_t entityID = TU_U16_HIGH(p_request->wIndex);
    switch (ctrlSel) {
      case AUDIO_FU_CTRL_MUTE: {
        // Audio control mute cur parameter block consists of only one byte - we
        // thus can send it right away There does not exist a range parameter
        // block for mute
        LOG_AUDIO_DEBUG("    Get Mute of channel: %u", channelNum);
        bool current_mute = _mute[channelNum];
        bool rc = tud_control_xfer(rhport, p_request, &current_mute, 1);
        _mute[channelNum] = current_mute;
        return rc;
      }

      case AUDIO_FU_CTRL_VOLUME:
        switch (p_request->bRequest) {
          case AUDIO_CS_REQ_CUR:
            LOG_AUDIO_DEBUG("    Get Volume of channel: %u", channelNum);
            return tud_control_xfer(rhport, p_request, &_volume[channelNum],
                                    sizeof(_volume[channelNum]));

          case AUDIO_CS_REQ_RANGE:
            LOG_AUDIO_DEBUG("    Get Volume range of channel: %u", channelNum);

            // Copy values - only for testing - better is version below
            audio_control_range_2_n_t(1) ret;

            ret.wNumSubRanges = 1;
            ret.subrange[0].bMin = 0;    // -90 dB
            ret.subrange[0].bMax = 100;  // +90 dB
            ret.subrange[0].bRes = 1;    // 1 dB steps

            return _api.tud_audio_buffer_and_schedule_control_xfer(
                rhport, p_request, (void *)&ret, sizeof(ret));

            // Unknown/Unsupported control
          default:
            // TU_BREAKPOINT();
            return false;
        }
        break;

        // Unknown/Unsupported control
      default:
        // TU_BREAKPOINT();
        return false;
    }
    return false;
  }

  virtual bool feature_unit_set_request(uint8_t rhport,
                                        tusb_control_request_t const *p_request,
                                        uint8_t const *buf) {
    (void)rhport;
    audio_control_request_t const *request =
        (audio_control_request_t const *)p_request;

    // TU_ASSERT(request->bEntityID == UAC2_ENTITY_SPK_FEATURE_UNIT);
    TU_VERIFY(request->bRequest == AUDIO_CS_REQ_CUR);

    if (request->bControlSelector == AUDIO_FU_CTRL_MUTE) {
      TU_VERIFY(request->wLength == sizeof(audio_control_cur_1_t));

      _mute[request->bChannelNumber] =
          ((audio_control_cur_1_t const *)buf)->bCur;

      LOG_AUDIO_DEBUG("Set channel %d Mute: %d", request->bChannelNumber,
                      _mute[request->bChannelNumber]);

      return true;
    } else if (request->bControlSelector == AUDIO_FU_CTRL_VOLUME) {
      TU_VERIFY(request->wLength == sizeof(audio_control_cur_2_t));

      _volume[request->bChannelNumber] =
          ((audio_control_cur_2_t const *)buf)->bCur;

      LOG_AUDIO_DEBUG("Set channel %d volume: %d dB", request->bChannelNumber,
                      _volume[request->bChannelNumber] / 256);

      return true;
    } else {
      LOG_AUDIO_DEBUG(
          "Feature unit set request not supported, entity = %u, selector = %u, "
          "request = %u",
          request->bEntityID, request->bControlSelector, request->bRequest);
      return false;
    }
  }

  virtual bool clock_get_request(uint8_t rhport,
                                 tusb_control_request_t const *p_request) {
    uint8_t channelNum = TU_U16_LOW(p_request->wValue);
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    // uint8_t itf = TU_U16_LOW(p_request->wIndex); 			// Since
    // we have only one audio function implemented, we do not need the itf value
    uint8_t entityID = TU_U16_HIGH(p_request->wIndex);
    switch (ctrlSel) {
      case AUDIO_CS_CTRL_SAM_FREQ:
        // channelNum is always zero in this case
        switch (p_request->bRequest) {
          case AUDIO_CS_REQ_CUR:
            LOG_AUDIO_DEBUG("    Get Sample Freq. -> %d", cfg.sample_rate);
            // Buffered control transfer is needed for IN flow control to work
            return _api.tud_audio_buffer_and_schedule_control_xfer(
                rhport, p_request, &cfg.sample_rate, sizeof(cfg.sample_rate));

          case AUDIO_CS_REQ_RANGE:
            LOG_AUDIO_DEBUG("    Get Sample Freq. range -> %d - %d",
                            cfg.sample_rate, cfg.sample_rate);
            audio_control_range_4_n_t(2) sampleFreqRng;
            sampleFreqRng.wNumSubRanges = 1;
            sampleFreqRng.subrange[0].bMin = cfg.sample_rate;
            sampleFreqRng.subrange[0].bMax = cfg.sample_rate;
            sampleFreqRng.subrange[0].bRes = 0;
            // sampleFreqRng.subrange[1].bMin = cfg.sample_rate;
            // sampleFreqRng.subrange[1].bMax = AUDIO_FREQ_MAX;
            // sampleFreqRng.subrange[1].bRes = 0;
            return tud_control_xfer(rhport, p_request, &sampleFreqRng,
                                    sizeof(sampleFreqRng));

          // Unknown/Unsupported control
          default:
            // TU_BREAKPOINT();
            return false;
        }
        break;

      case AUDIO_CS_CTRL_CLK_VALID:
        // Only cur attribute exists for this request
        LOG_AUDIO_DEBUG("    Get Sample Freq. valid");
        return tud_control_xfer(rhport, p_request, &_clk_is_valid,
                                sizeof(_clk_is_valid));

      // Unknown/Unsupported control
      default:
        // TU_BREAKPOINT();
        return false;
    }
  }

  virtual bool clock_set_request(uint8_t rhport,
                                 tusb_control_request_t const *p_request,
                                 uint8_t const *buf) {
    (void)rhport;
    uint8_t channelNum = TU_U16_LOW(p_request->wValue);
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    // uint8_t itf = TU_U16_LOW(p_request->wIndex); 			// Since
    // we have only one audio function implemented, we do not need the itf value
    uint8_t entityID = TU_U16_HIGH(p_request->wIndex);

    // TU_ASSERT(request->bEntityID == UAC2_ENTITY_CLOCK);
    // TU_VERIFY(p_request->bRequest == AUDIO_CS_REQ_CUR);

    if (ctrlSel == AUDIO_CS_CTRL_SAM_FREQ) {
      TU_VERIFY(p_request->wLength == sizeof(audio_control_cur_4_t));
      cfg.sample_rate = (uint32_t)((audio_control_cur_4_t const *)buf)->bCur;
      LOG_AUDIO_DEBUG("Clock set current freq: %ld", cfg.sample_rate);
      return true;
    } else {
      LOG_AUDIO_DEBUG(
          "Clock set request not supported, entity = %u, selector = %u, "
          "request "
          "= %u",
          entityID, ctrlSel, p_request->bRequest);
      return false;
    }
  }

  uint16_t interfaceDescriptor(uint8_t *buf, uint16_t bufsize) {
    // Generate the descriptor and calculate it's length
    uint8_t feature_unit_len = getFeatureUnitLength();
    _desc_append_pos = 0;

    // setup control interface and endpoint
    if (_desc_len == 0) {
      _itf_number_total = 1;
      _itfnum_ctl = allocInterface();
      _ep_ctl = allocEndpoint(TUSB_DIR_IN);
    }

    // Setup endpints and interfaces
    if (isHeadset() && _desc_len == 0) {
      _itfnum_mic = allocInterface();         // input interface
      _itfnum_spk = allocInterface();         // output interface
      _ep_mic = allocEndpoint(TUSB_DIR_IN);   // intput
      _ep_int = allocEndpoint(TUSB_DIR_IN);   // input
      _ep_spk = allocEndpoint(TUSB_DIR_OUT);  // output
      _itf_number_total += 2;
    } else if (isMicrophone() && _desc_len == 0) {
      _itfnum_mic = allocInterface();
      _ep_mic = allocEndpoint(TUSB_DIR_IN);
      _itf_number_total++;
    } else if (isSpeaker() && _desc_len == 0) {
      _itfnum_spk = allocInterface();  // output interface
      _ep_spk = allocEndpoint(TUSB_DIR_OUT);
      _ep_fb = allocEndpoint(TUSB_DIR_IN);
      _itf_number_total++;
    }

    // generate descriptor
    if (isHeadset()) {
      uint8_t total_len = TUD_AUDIO_DESC_CLK_SRC_LEN + feature_unit_len +
                          (2 * (TUD_AUDIO_DESC_INPUT_TERM_LEN +
                                TUD_AUDIO_DESC_OUTPUT_TERM_LEN));
      interfaceDescriptorHeadset(buf, total_len);
    } else if (isMicrophone()) {
      uint8_t total_len = TUD_AUDIO_DESC_CLK_SRC_LEN + feature_unit_len +
                          TUD_AUDIO_DESC_INPUT_TERM_LEN +
                          TUD_AUDIO_DESC_OUTPUT_TERM_LEN;
      interfaceDescriptorMicrophone(buf, total_len);
    } else if (isSpeaker()) {
      uint8_t total_len = TUD_AUDIO_DESC_CLK_SRC_LEN + feature_unit_len +
                          TUD_AUDIO_DESC_INPUT_TERM_LEN +
                          TUD_AUDIO_DESC_OUTPUT_TERM_LEN;
      interfaceDescriptorSpeaker(buf, total_len);
    }

    // record descriptor length
    if (_desc_len == 0) {
      _desc_len = _desc_append_pos;
    }

    return _desc_len;
  }

  void interfaceDescriptorHeader(uint8_t *buf, uint8_t total_len,
                                 uint8_t category) {
    /* Standard Interface Association Descriptor (IAD) */
    uint8_t d1[] = {TUD_AUDIO_DESC_IAD(/*_firstitfs*/ _itfnum_ctl,
                                       /*_nitfs*/ _itf_number_total,
                                       /*_stridx*/ 0)};
    append(buf, d1, sizeof(d1));

    /* Standard AC Interface Descriptor(4.7.1) */
    uint8_t d2[] = {TUD_AUDIO_DESC_STD_AC(/*_itfnum*/ _itfnum_ctl,
                                          /*_nEPs*/ 0x00, /*_stridx*/ _stridx)};
    append(buf, d2, sizeof(d2));

    /* Class-Specific AC Interface Header Descriptor(4.7.2)  AUDIO_FUNC_OTHER */
    uint8_t d3[] = {TUD_AUDIO_DESC_CS_AC(
        /*_bcdADC*/ 0x0200, /*_category*/ category, /*_totallen*/ total_len,
        /*_ctrl*/ AUDIO_CTRL_NONE << AUDIO_CS_AS_INTERFACE_CTRL_LATENCY_POS)};
    append(buf, d3, sizeof(d3));
  }
  void interfaceDescriptorMicrophone(uint8_t *buf, uint8_t total_len) {
    interfaceDescriptorHeader(buf, total_len, AUDIO_FUNC_MICROPHONE);
    /* Clock Source Descriptor(4.7.2.1) */
    //   TUD_AUDIO_DESC_CLK_SRC(/*_clkid*/ 0x04, /*_attr*/
    //   AUDIO_CLOCK_SOURCE_ATT_INT_FIX_CLK, /*_ctrl*/ (AUDIO_CTRL_R <<
    //   AUDIO_CLOCK_SOURCE_CTRL_CLK_FRQ_POS), /*_assocTerm*/ 0x01,  /*_stridx*/
    //   0x00),
    uint8_t d4[] = {TUD_AUDIO_DESC_CLK_SRC(
        /*_clkid*/ UAC2_ENTITY_CLOCK,
        /*_attr*/ AUDIO_CLOCK_SOURCE_ATT_INT_FIX_CLK,
        /*_ctrl*/ (AUDIO_CTRL_R << AUDIO_CLOCK_SOURCE_CTRL_CLK_FRQ_POS),
        /*_assocTerm*/ UAC2_ENTITY_MIC_INPUT_TERMINAL, /*_stridx*/ 0x00)};
    append(buf, d4, sizeof(d4));

    /* Input Terminal Descriptor(4.7.2.4) */
    // TUD_AUDIO_DESC_INPUT_TERM(/*_termid*/ 0x01, /*_termtype*/
    // AUDIO_TERM_TYPE_IN_GENERIC_MIC, /*_assocTerm*/ 0x03, /*_clkid*/ 0x04,
    // /*_nchannelslogical*/ 0x02, /*_channelcfg*/
    // AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, /*_idxchannelnames*/ 0x00, /*_ctrl*/
    // AUDIO_CTRL_R << AUDIO_IN_TERM_CTRL_CONNECTOR_POS, /*_stridx*/ 0x00),
    uint8_t d7[] = {TUD_AUDIO_DESC_INPUT_TERM(
        /*_termid*/ UAC2_ENTITY_MIC_INPUT_TERMINAL,
        /*_termtype*/ UAC2_ENTITY_SPK_OUTPUT_TERMINAL,
        /*_assocTerm*/ UAC2_ENTITY_MIC_OUTPUT_TERMINAL,
        /*_clkid*/ UAC2_ENTITY_CLOCK, /*_nchannelslogical*/ cfg.channels,
        /*_channelcfg*/ AUDIO_CHANNEL_CONFIG_NON_PREDEFINED,
        /*_idxchannelnames*/ 0x00,
        /*_ctrl*/ AUDIO_CTRL_R << AUDIO_IN_TERM_CTRL_CONNECTOR_POS,
        /*_stridx*/ 0x00)};
    append(buf, d7, sizeof(d7));
    /* Output Terminal Descriptor(4.7.2.5) */
    // TUD_AUDIO_DESC_OUTPUT_TERM(/*_termid*/ 0x03, /*_termtype*/
    // AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ 0x01, /*_srcid*/ 0x02,
    // /*_clkid*/ 0x04, /*_ctrl*/ 0x0000, /*_stridx*/ 0x00),
    uint8_t d8[] = {TUD_AUDIO_DESC_OUTPUT_TERM(
        /*_termid*/ UAC2_ENTITY_MIC_OUTPUT_TERMINAL,
        /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING,
        /*_assocTerm*/ UAC2_ENTITY_SPK_INPUT_TERMINAL,
        /*_srcid*/ UAC2_ENTITY_MIC_FEATURE_UNIT, /*_clkid*/ UAC2_ENTITY_CLOCK,
        /*_ctrl*/ 0x0000, /*_stridx*/ 0x00)};
    append(buf, d8, sizeof(d8));

    /* Feature Unit Descriptor(4.7.2.8) */
    // #define TUD_AUDIO_DESC_FEATURE_UNIT_TWO_CHANNEL(_unitid, _srcid,
    // _ctrlch0master, _ctrlch1, _ctrlch2, _stridx)
    // TUD_AUDIO_DESC_FEATURE_UNIT_TWO_CHANNEL_LEN, TUSB_DESC_CS_INTERFACE,
    // AUDIO_CS_AC_INTERFACE_FEATURE_UNIT, _unitid, _srcid,
    // U32_TO_U8S_LE(_ctrlch0master), U32_TO_U8S_LE(_ctrlch1),
    // U32_TO_U8S_LE(_ctrlch2), _stridx
    // TUD_AUDIO_DESC_FEATURE_UNIT_TWO_CHANNEL(/*_unitid*/ 0x02, /*_srcid*/
    // 0x01, /*_ctrlch0master*/ AUDIO_CTRL_RW <<
    // AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW <<
    // AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS, /*_ctrlch1*/ AUDIO_CTRL_RW <<
    // AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW <<
    // AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS, /*_ctrlch2*/ AUDIO_CTRL_RW <<
    // AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW <<
    // AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS,  /*_stridx*/ 0x00),\

    uint8_t feature_unit_len = getFeatureUnitLength();
    uint8_t df1[] = {feature_unit_len, TUSB_DESC_CS_INTERFACE,
                     AUDIO_CS_AC_INTERFACE_FEATURE_UNIT,
                     /*_unitid*/ UAC2_ENTITY_MIC_FEATURE_UNIT,
                     /*_srcid*/ UAC2_ENTITY_MIC_INPUT_TERMINAL};
    append(buf, df1, sizeof(df1));
    for (int j = 0; j < cfg.channels + 1; j++) {
      uint8_t df2[] = {
          U32_TO_U8S_LE(AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_MUTE_POS |
                        AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS)};
      append(buf, df2, sizeof(df2));
    }
    // _stridx
    uint8_t df3[1] = {0x00};
    append(buf, df3, sizeof(df3));

    /* Standard AS Interface Descriptor(4.9.1) */ /* Interface 2, Alternate 0 -
                                                     default alternate setting
                                                     with 0 bandwidth */
    //               TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/
    //               (uint8_t)((_itfnum)+1), /*_altset*/ 0x00, /*_nEPs*/ 0x00,
    //               /*_stridx*/ 0x00),
    uint8_t d15[] = {TUD_AUDIO_DESC_STD_AS_INT(
        /*_itfnum*/ (uint8_t)(_itfnum_mic), /*_altset*/ 0x00, /*_nEPs*/ 0x00,
        /*_stridx*/ 0x00)};
    append(buf, d15, sizeof(d15));
    /* Standard AS Interface Descriptor(4.9.1) */
    /* Interface 2, Alternate 1 - alternate interface for data streaming */
    //               TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/
    //               (uint8_t)((_itfnum)+1), /*_altset*/ 0x00, /*_nEPs*/ 0x00,
    //               /*_stridx*/ 0x00),
    uint8_t d16[] = {TUD_AUDIO_DESC_STD_AS_INT(
        /*_itfnum*/ (uint8_t)(_itfnum_mic), /*_altset*/ 0x01, /*_nEPs*/ 0x01,
        /*_stridx*/ 0x00)};
    append(buf, d16, sizeof(d16));
    /* Class-Specific AS Interface Descriptor(4.9.2) */
    //               TUD_AUDIO_DESC_CS_AS_INT(/*_termid*/ 0x03, /*_ctrl*/
    //               AUDIO_CTRL_NONE, /*_formattype*/ AUDIO_FORMAT_TYPE_I,
    //               /*_formats*/ AUDIO_DATA_FORMAT_TYPE_I_PCM,
    //               /*_nchannelsphysical*/ 0x02, /*_channelcfg*/
    //               AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, /*_stridx*/ 0x00),
    uint8_t d17[] = {TUD_AUDIO_DESC_CS_AS_INT(
        /*_termid*/ UAC2_ENTITY_MIC_OUTPUT_TERMINAL, /*_ctrl*/ AUDIO_CTRL_NONE,
        /*_formattype*/ AUDIO_FORMAT_TYPE_I,
        /*_formats*/ AUDIO_DATA_FORMAT_TYPE_I_PCM,
        /*_nchannelsphysical*/ cfg.channels,
        /*_channelcfg*/ AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, /*_stridx*/ 0x00)};
    append(buf, d17, sizeof(d17));
    /* Type I Format Type Descriptor(2.3.1.6 - Audio Formats) */
    //               TUD_AUDIO_DESC_TYPE_I_FORMAT(_nBytesPerSample,
    //               _nBitsUsedPerSample),
    uint8_t d18[] = {TUD_AUDIO_DESC_TYPE_I_FORMAT(
        (uint8_t)(cfg.bits_per_sample / 8), (uint8_t)cfg.bits_per_sample)};
    append(buf, d18, sizeof(d18));
    /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.10.1.1) */
    //               TUD_AUDIO_DESC_STD_AS_ISO_EP(/*_ep*/ _epin, /*_attr*/
    //               (uint8_t) ((uint8_t)TUSB_XFER_ISOCHRONOUS |
    //               (uint8_t)TUSB_ISO_EP_ATT_ASYNCHRONOUS |
    //               (uint8_t)TUSB_ISO_EP_ATT_DATA), /*_maxEPsize*/ _epsize,
    //               /*_interval*/ 0x01),
    uint8_t attr = static_cast<uint8_t>(TUSB_XFER_ISOCHRONOUS) |
                   static_cast<uint8_t>(TUSB_ISO_EP_ATT_ASYNCHRONOUS) |
                   static_cast<uint8_t>(TUSB_ISO_EP_ATT_DATA);
    uint8_t d19[] = {TUD_AUDIO_DESC_STD_AS_ISO_EP(
        /*_ep*/ _ep_mic, /*_attr*/ attr, /*_maxEPsize*/ getMaxEPSize(),
        /*_interval*/ 0x01)};
    append(buf, d19, sizeof(d19));
    /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.10.1.2) */
    //               TUD_AUDIO_DESC_CS_AS_ISO_EP(/*_attr*/
    //               AUDIO_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, /*_ctrl*/
    //               AUDIO_CTRL_NONE, /*_lockdelayunit*/
    //               AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_UNDEFINED,
    //               /*_lockdelay*/ 0x0000)
    uint8_t d20[] = {TUD_AUDIO_DESC_CS_AS_ISO_EP(
        /*_attr*/ AUDIO_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK,
        /*_ctrl*/ AUDIO_CTRL_NONE,
        /*_lockdelayunit*/ AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_UNDEFINED,
        /*_lockdelay*/ 0x0000)};
    append(buf, d20, sizeof(d20));
  }

  void interfaceDescriptorSpeaker(uint8_t *buf, uint8_t total_len) {
    interfaceDescriptorHeader(buf, total_len, AUDIO_FUNC_DESKTOP_SPEAKER);
    /* Clock Source Descriptor(4.7.2.1) */
    //   TUD_AUDIO_DESC_CLK_SRC(/*_clkid*/ 0x04, /*_attr*/
    //   AUDIO_CLOCK_SOURCE_ATT_INT_FIX_CLK, /*_ctrl*/ (AUDIO_CTRL_R <<
    //   AUDIO_CLOCK_SOURCE_CTRL_CLK_FRQ_POS), /*_assocTerm*/ 0x01,  /*_stridx*/
    //   0x00),
    uint8_t d4[] = {TUD_AUDIO_DESC_CLK_SRC(
        /*_clkid*/ UAC2_ENTITY_CLOCK,
        /*_attr*/ AUDIO_CLOCK_SOURCE_ATT_INT_PRO_CLK,
        /*_ctrl*/ (AUDIO_CTRL_RW << AUDIO_CLOCK_SOURCE_CTRL_CLK_FRQ_POS),
        /*_assocTerm*/ UAC2_ENTITY_SPK_INPUT_TERMINAL, /*_stridx*/ 0x00)};
    append(buf, d4, sizeof(d4));
    /* Input Terminal Descriptor(4.7.2.4) */
    //              TUD_AUDIO_DESC_INPUT_TERM(/*_termid*/ 0x01, /*_termtype*/
    //              AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ 0x00,
    //              /*_clkid*/ 0x04, /*_nchannelslogical*/ 0x02, /*_channelcfg*/
    //              AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, /*_idxchannelnames*/
    //              0x00, /*_ctrl*/ 0 * (AUDIO_CTRL_R <<
    //              AUDIO_IN_TERM_CTRL_CONNECTOR_POS), /*_stridx*/ 0x00),
    uint8_t d7[] = {TUD_AUDIO_DESC_INPUT_TERM(
        /*_termid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL,
        /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ 0x00,
        /*_clkid*/ UAC2_ENTITY_CLOCK, /*_nchannelslogical*/ cfg.channels,
        /*_channelcfg*/ AUDIO_CHANNEL_CONFIG_NON_PREDEFINED,
        /*_idxchannelnames*/ 0x00,
        /*_ctrl*/ 0 * (AUDIO_CTRL_R << AUDIO_IN_TERM_CTRL_CONNECTOR_POS),
        /*_stridx*/ 0x00)};
    append(buf, d7, sizeof(d7));
    /* Output Terminal Descriptor(4.7.2.5) */
    //              TUD_AUDIO_DESC_OUTPUT_TERM(/*_termid*/ 0x03, /*_termtype*/
    //              AUDIO_TERM_TYPE_OUT_DESKTOP_SPEAKER, /*_assocTerm*/ 0x01,
    //              /*_srcid*/ 0x02, /*_clkid*/ 0x04, /*_ctrl*/ 0x0000,
    //              /*_stridx*/ 0x00),
    uint8_t d8[] = {TUD_AUDIO_DESC_OUTPUT_TERM(
        /*_termid*/ UAC2_ENTITY_SPK_OUTPUT_TERMINAL,
        /*_termtype*/ AUDIO_TERM_TYPE_OUT_DESKTOP_SPEAKER,
        /*_assocTerm*/ UAC2_ENTITY_SPK_INPUT_TERMINAL,
        /*_srcid*/ UAC2_ENTITY_SPK_FEATURE_UNIT, /*_clkid*/ UAC2_ENTITY_CLOCK,
        /*_ctrl*/ 0x0000, /*_stridx*/ 0x00)};
    append(buf, d8, sizeof(d8));

    /* Feature Unit Descriptor(4.7.2.8) */
    // #define TUD_AUDIO_DESC_FEATURE_UNIT_TWO_CHANNEL(_unitid, _srcid,
    // _ctrlch0master, _ctrlch1, _ctrlch2, _stridx)
    // TUD_AUDIO_DESC_FEATURE_UNIT_TWO_CHANNEL_LEN, TUSB_DESC_CS_INTERFACE,
    // AUDIO_CS_AC_INTERFACE_FEATURE_UNIT, _unitid, _srcid,
    // U32_TO_U8S_LE(_ctrlch0master), U32_TO_U8S_LE(_ctrlch1),
    // U32_TO_U8S_LE(_ctrlch2), _stridx
    // TUD_AUDIO_DESC_FEATURE_UNIT_TWO_CHANNEL(/*_unitid*/ 0x02, /*_srcid*/
    // 0x01, /*_ctrlch0master*/ AUDIO_CTRL_RW <<
    // AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW <<
    // AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS, /*_ctrlch1*/ AUDIO_CTRL_RW <<
    // AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW <<
    // AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS, /*_ctrlch2*/ AUDIO_CTRL_RW <<
    // AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW <<
    // AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS,/*_stridx*/ 0x00),\ uint8_t dfu[] =
    // {TUD_AUDIO_DESC_FEATURE_UNIT_TWO_CHANNEL(/*_unitid*/
    // UAC2_ENTITY_SPK_FEATURE_UNIT, /*_srcid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL,
    // /*_ctrlch0master*/ AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_MUTE_POS |
    // AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS, /*_ctrlch1*/
    // AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW <<
    // AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS, /*_ctrlch2*/ AUDIO_CTRL_RW <<
    // AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW <<
    // AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS,  /*_stridx*/ 0x00)}; append(buf, dfu,
    // sizeof(dfu));

    uint8_t feature_unit_len = getFeatureUnitLength();
    uint8_t df1[] = {feature_unit_len, TUSB_DESC_CS_INTERFACE,
                     AUDIO_CS_AC_INTERFACE_FEATURE_UNIT,
                     /*_unitid*/ UAC2_ENTITY_SPK_FEATURE_UNIT,
                     /*_srcid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL};
    append(buf, df1, sizeof(df1));
    for (int j = 0; j < cfg.channels + 1; j++) {
      uint8_t df2[] = {
          U32_TO_U8S_LE(AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_MUTE_POS |
                        AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS)};
      append(buf, df2, sizeof(df2));
    }
    /*_stridx 0x00*/
    uint8_t df3[1] = {0x00};
    append(buf, df3, sizeof(df3));

    /* Standard AS Interface Descriptor(4.9.1) */ /* Interface 2, Alternate 0 -
                                                     default alternate setting
                                                     with 0 bandwidth */
    //                TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)((_itfnum)
    //                + 1), /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ 0x00),
    uint8_t d15[] = {TUD_AUDIO_DESC_STD_AS_INT(
        /*_itfnum*/ (uint8_t)(_itfnum_spk), /*_altset*/ 0x00, /*_nEPs*/ 0x00,
        /*_stridx*/ 0x00)};
    append(buf, d15, sizeof(d15));
    /* Standard AS Interface Descriptor(4.9.1) */
    /* Interface 2, Alternate 1 - alternate interface for data streaming */
    //                TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)((_itfnum)
    //                + 1), /*_altset*/ 0x01, /*_nEPs*/ 0x02, /*_stridx*/ 0x00),
    uint8_t d16[] = {TUD_AUDIO_DESC_STD_AS_INT(
        /*_itfnum*/ (uint8_t)(_itfnum_spk), /*_altset*/ 0x01, /*_nEPs*/ 0x02,
        /*_stridx*/ 0x00)};
    append(buf, d16, sizeof(d16));
    // //               TUD_AUDIO_DESC_CS_AS_INT(/*_termid*/ 0x03, /*_ctrl*/
    // AUDIO_CTRL_NONE, /*_formattype*/ AUDIO_FORMAT_TYPE_I, /*_formats*/
    // AUDIO_DATA_FORMAT_TYPE_I_PCM, /*_nchannelsphysical*/ 0x02,
    // /*_channelcfg*/ AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, /*_stridx*/ 0x00),
    uint8_t d17[] = {TUD_AUDIO_DESC_CS_AS_INT(
        /*_termid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL, /*_ctrl*/ AUDIO_CTRL_NONE,
        /*_formattype*/ AUDIO_FORMAT_TYPE_I,
        /*_formats*/ AUDIO_DATA_FORMAT_TYPE_I_PCM,
        /*_nchannelsphysical*/ cfg.channels,
        /*_channelcfg*/ AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, /*_stridx*/ 0x00)};
    append(buf, d17, sizeof(d17));

    /* Type I Format Type Descriptor(2.3.1.6 - Audio Formats) */
    //               TUD_AUDIO_DESC_TYPE_I_FORMAT(_nBytesPerSample,
    //               _nBitsUsedPerSample),
    uint8_t d18[] = {TUD_AUDIO_DESC_TYPE_I_FORMAT(
        (uint8_t)(cfg.bits_per_sample / 8), (uint8_t)cfg.bits_per_sample)};
    append(buf, d18, sizeof(d18));
    // /* Class-Specific AS Interface Descriptor(4.9.2) */
    /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.10.1.1) */
    //               TUD_AUDIO_DESC_STD_AS_ISO_EP(/*_ep*/ _epout, /*_attr*/
    //               (uint8_t) ((uint8_t)TUSB_XFER_ISOCHRONOUS |
    //               (uint8_t)TUSB_ISO_EP_ATT_ASYNCHRONOUS |
    //               (uint8_t)TUSB_ISO_EP_ATT_DATA), /*_maxEPsize*/ _epoutsize,
    //               /*_interval*/ 0x01),
    uint8_t d19[] = {TUD_AUDIO_DESC_STD_AS_ISO_EP(
        /*_ep*/ _ep_spk, /*_attr*/
        (uint8_t)((uint8_t)TUSB_XFER_ISOCHRONOUS |
                  (uint8_t)TUSB_ISO_EP_ATT_ASYNCHRONOUS |
                  (uint8_t)TUSB_ISO_EP_ATT_DATA),
        /*_maxEPsize*/ getMaxEPSize(), /*_interval*/ 0x01)};
    append(buf, d19, sizeof(d19));
    /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.10.1.2) */
    //               TUD_AUDIO_DESC_CS_AS_ISO_EP(/*_attr*/
    //               AUDIO_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, /*_ctrl*/
    //               AUDIO_CTRL_NONE, /*_lockdelayunit*/
    //               AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC,
    //               /*_lockdelay*/ 0x0001)
    uint8_t d20[] = {TUD_AUDIO_DESC_CS_AS_ISO_EP(
        /*_attr*/ AUDIO_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK,
        /*_ctrl*/ AUDIO_CTRL_NONE,
        /*_lockdelayunit*/ AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC,
        /*_lockdelay*/ 0x0001)};
    append(buf, d20, sizeof(d20));

    // /* Standard AS Isochronous Feedback Endpoint Descriptor(4.10.2.1) */
#if ISO_FB_EP==3
    // TUD_AUDIO_DESC_STD_AS_ISO_FB_EP(/*_ep*/ _epfb, /*_epsize*/ _epfbsize, /*_interval*/ TUD_OPT_HIGH_SPEED ? 4 : 1)
    uint8_t d21[] = {TUD_AUDIO_DESC_STD_AS_ISO_FB_EP(
        /*_ep*/ _ep_fb, /*_epsize*/ 0X04,
        /*_interval*/ TUD_OPT_HIGH_SPEED ? 4 : 1)};
#else
    uint8_t d21[] = {TUD_AUDIO_DESC_STD_AS_ISO_FB_EP(
        /*_ep*/ _ep_fb, /*_epsize 0X04,*/
        /*_interval*/ TUD_OPT_HIGH_SPEED ? 4 : 1)};
#endif
    append(buf, d21, sizeof(d21));
  }

  void interfaceDescriptorHeadset(uint8_t *buf, uint8_t total_len) {
    interfaceDescriptorHeader(buf, total_len, AUDIO_FUNC_HEADSET);

    /* Clock Source Descriptor(4.7.2.1) */
    // TUD_AUDIO_DESC_CLK_SRC(/*_clkid*/ UAC2_ENTITY_CLOCK, /*_attr*/ 3,
    // /*_ctrl*/ 7, /*_assocTerm*/ 0x00,  /*_stridx*/ 0x00),
    uint8_t d1[] = {TUD_AUDIO_DESC_CLK_SRC(
        /*_clkid*/ UAC2_ENTITY_CLOCK,
        /*_attr*/ AUDIO_CLOCK_SOURCE_ATT_INT_PRO_CLK, /*_ctrl*/ 7,
        /*_assocTerm*/ 00, /*_stridx*/ 0x00)};
    append(buf, d1, sizeof(d1));

    /* Input Terminal Descriptor(4.7.2.4) */
    // TUD_AUDIO_DESC_INPUT_TERM(/*_termid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL,
    // /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ 0x00,
    // /*_clkid*/ UAC2_ENTITY_CLOCK, /*_nchannelslogical*/ 0x02, /*_channelcfg*/
    // AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, /*_idxchannelnames*/ 0x00, /*_ctrl*/
    // 0 * (AUDIO_CTRL_R << AUDIO_IN_TERM_CTRL_CONNECTOR_POS), /*_stridx*/
    // 0x00),
    uint8_t d2[] = {TUD_AUDIO_DESC_INPUT_TERM(
        /*_termid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL,
        /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ 0x00,
        /*_clkid*/ UAC2_ENTITY_CLOCK, /*_nchannelslogical*/ cfg.channels,
        /*_channelcfg*/ AUDIO_CHANNEL_CONFIG_NON_PREDEFINED,
        /*_idxchannelnames*/ 0x00,
        /*_ctrl*/ 0 * (AUDIO_CTRL_R << AUDIO_IN_TERM_CTRL_CONNECTOR_POS),
        /*_stridx*/ 0x00)};
    append(buf, d2, sizeof(d2));

    /* Feature Unit Descriptor(4.7.2.8) */
    // TUD_AUDIO_DESC_FEATURE_UNIT_TWO_CHANNEL(/*_unitid*/
    // UAC2_ENTITY_SPK_FEATURE_UNIT, /*_srcid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL,
    // /*_ctrlch0master*/ (AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_MUTE_POS |
    // AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS), /*_ctrlch1*/
    // (AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW <<
    // AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS), /*_ctrlch2*/ (AUDIO_CTRL_RW <<
    // AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW <<
    // AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS), /*_stridx*/ 0x00),
    uint8_t feature_unit_len = getFeatureUnitLength();
    uint8_t df1[] = {feature_unit_len, TUSB_DESC_CS_INTERFACE,
                     AUDIO_CS_AC_INTERFACE_FEATURE_UNIT,
                     /*_unitid*/ UAC2_ENTITY_SPK_FEATURE_UNIT,
                     /*_srcid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL};
    append(buf, df1, sizeof(df1));
    // first is master channel
    for (int j = 0; j < cfg.channels + 1; j++) {
      uint8_t df2[] = {
          U32_TO_U8S_LE((AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_MUTE_POS |
                         AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS))};
      append(buf, df2, sizeof(df2));
    }
    /*_stridx 0x00*/
    uint8_t df3[1] = {0x00};
    append(buf, df3, sizeof(df3));

    //-- out ---

    /* Output Terminal Descriptor(4.7.2.5) */
    // TUD_AUDIO_DESC_OUTPUT_TERM(/*_termid*/ UAC2_ENTITY_SPK_OUTPUT_TERMINAL,
    // /*_termtype*/ AUDIO_TERM_TYPE_OUT_HEADPHONES, /*_assocTerm*/ 0x00,
    // /*_srcid*/ UAC2_ENTITY_SPK_FEATURE_UNIT, /*_clkid*/ UAC2_ENTITY_CLOCK,
    // /*_ctrl*/ 0x0000, /*_stridx*/ 0x00),
    uint8_t d3[] = {TUD_AUDIO_DESC_OUTPUT_TERM(
        /*_termid*/ UAC2_ENTITY_SPK_OUTPUT_TERMINAL,
        /*_termtype*/ AUDIO_TERM_TYPE_OUT_HEADPHONES, /*_assocTerm*/ 00,
        /*_srcid*/ UAC2_ENTITY_SPK_FEATURE_UNIT, /*_clkid*/ UAC2_ENTITY_CLOCK,
        /*_ctrl*/ 0x0000, /*_stridx*/ 0x00)};
    append(buf, d3, sizeof(d3));

    /* Input Terminal Descriptor(4.7.2.4) */
    // TUD_AUDIO_DESC_INPUT_TERM(/*_termid*/ UAC2_ENTITY_MIC_INPUT_TERMINAL,
    // /*_termtype*/ AUDIO_TERM_TYPE_IN_GENERIC_MIC, /*_assocTerm*/ 0x00,
    // /*_clkid*/ UAC2_ENTITY_CLOCK, /*_nchannelslogical*/ 0x01, /*_channelcfg*/
    // AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, /*_idxchannelnames*/ 0x00, /*_ctrl*/
    // 0 * (AUDIO_CTRL_R << AUDIO_IN_TERM_CTRL_CONNECTOR_POS), /*_stridx*/
    // 0x00),
    uint8_t d4[] = {TUD_AUDIO_DESC_INPUT_TERM(
        /*_termid*/ UAC2_ENTITY_MIC_INPUT_TERMINAL,
        /*_termtype*/ AUDIO_TERM_TYPE_IN_GENERIC_MIC, /*_assocTerm*/ 00,
        /*_clkid*/ UAC2_ENTITY_CLOCK, /*_nchannelslogical*/ cfg.channels,
        /*_channelcfg*/ AUDIO_CHANNEL_CONFIG_NON_PREDEFINED,
        /*_idxchannelnames*/ 0x00,
        /*_ctrl*/ 0 * (AUDIO_CTRL_R << AUDIO_IN_TERM_CTRL_CONNECTOR_POS),
        /*_stridx*/ 0x00)};
    append(buf, d4, sizeof(d4));

    /* Output Terminal Descriptor(4.7.2.5) */
    // TUD_AUDIO_DESC_OUTPUT_TERM(/*_termid*/ UAC2_ENTITY_MIC_OUTPUT_TERMINAL,
    // /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ 0x00,
    // /*_srcid*/ UAC2_ENTITY_MIC_INPUT_TERMINAL, /*_clkid*/ UAC2_ENTITY_CLOCK,
    // /*_ctrl*/ 0x0000, /*_stridx*/ 0x00),
    uint8_t d5[] = {TUD_AUDIO_DESC_OUTPUT_TERM(
        /*_termid*/ UAC2_ENTITY_MIC_OUTPUT_TERMINAL,
        /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ 00,
        /*_srcid*/ UAC2_ENTITY_MIC_INPUT_TERMINAL, /*_clkid*/ UAC2_ENTITY_CLOCK,
        /*_ctrl*/ 0x0000, /*_stridx*/ 0x00)};
    append(buf, d5, sizeof(d5));

    /* Standard AC Interrupt Endpoint Descriptor(4.8.2.1) */
    // TUD_AUDIO_DESC_STD_AC_INT_EP(/*_ep*/ _epint, /*_interval*/ 0x01),
    uint8_t d6[] = {
        TUD_AUDIO_DESC_STD_AC_INT_EP(/*_ep*/ _ep_int, /*_interval*/ 0x01)};
    append(buf, d6, sizeof(d6));

    // -- SPK ---

    /* Standard AS Interface Descriptor(4.9.1) */
    /* Interface 1, Alternate 0 - default alternate setting with 0 bandwidth */
    // TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/
    // (uint8_t)(ITF_NUM_AUDIO_STREAMING_SPK), /*_altset*/ 0x00, /*_nEPs*/ 0x00,
    // /*_stridx*/ 0x05),
    uint8_t d7[] = {TUD_AUDIO_DESC_STD_AS_INT(
        /*_itfnum*/ (uint8_t)(_itfnum_spk), /*_altset*/ 0x00, /*_nEPs*/ 0x00,
        /*_stridx*/ 0x05)};
    append(buf, d7, sizeof(d7));

    /* Standard AS Interface Descriptor(4.9.1) */
    /* Interface 1, Alternate 1 - alternate interface for data streaming */
    // TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/
    // (uint8_t)(ITF_NUM_AUDIO_STREAMING_SPK), /*_altset*/ 0x01, /*_nEPs*/ 0x01,
    // /*_stridx*/ 0x05),
    uint8_t d8[] = {TUD_AUDIO_DESC_STD_AS_INT(
        /*_itfnum*/ (uint8_t)(_itfnum_spk), /*_altset*/ 0x01, /*_nEPs*/ 0x01,
        /*_stridx*/ 0x05)};
    append(buf, d8, sizeof(d8));

    /* Class-Specific AS Interface Descriptor(4.9.2) */
    // TUD_AUDIO_DESC_CS_AS_INT(/*_termid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL,
    // /*_ctrl*/ AUDIO_CTRL_NONE, /*_formattype*/ AUDIO_FORMAT_TYPE_I,
    // /*_formats*/ AUDIO_DATA_FORMAT_TYPE_I_PCM, /*_nchannelsphysical*/
    // CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX, /*_channelcfg*/
    // AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, /*_stridx*/ 0x00),
    uint8_t d9[] = {TUD_AUDIO_DESC_CS_AS_INT(
        /*_termid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL, /*_ctrl*/ AUDIO_CTRL_NONE,
        /*_formattype*/ AUDIO_FORMAT_TYPE_I,
        /*_formats*/ AUDIO_DATA_FORMAT_TYPE_I_PCM,
        /*_nchannelsphysical*/ cfg.channels,
        /*_channelcfg*/ AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, /*_stridx*/ 0x00)};
    append(buf, d9, sizeof(d9));

    /* Type I Format Type Descriptor(2.3.1.6 - Audio Formats) */
    // TUD_AUDIO_DESC_TYPE_I_FORMAT(CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_RX,
    // CFG_TUD_AUDIO_FUNC_1_FORMAT_1_RESOLUTION_RX),
    uint8_t d10[] = {TUD_AUDIO_DESC_TYPE_I_FORMAT(
        (uint8_t)(cfg.bits_per_sample / 8), (uint8_t)cfg.bits_per_sample)};
    append(buf, d10, sizeof(d10));

    /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.10.1.1) */
    // TUD_AUDIO_DESC_STD_AS_ISO_EP(/*_ep*/ _epout, /*_attr*/ (uint8_t)
    // ((uint8_t)TUSB_XFER_ISOCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_ADAPTIVE |
    // (uint8_t)TUSB_ISO_EP_ATT_DATA), /*_maxEPsize*/
    // TUD_AUDIO_EP_SIZE(CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE,
    // CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_RX,
    // CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX), /*_interval*/ 0x01),
    uint8_t d11[] = {TUD_AUDIO_DESC_STD_AS_ISO_EP(
        /*_ep*/ _ep_spk, /*_attr*/
        (uint8_t)((uint8_t)TUSB_XFER_ISOCHRONOUS |
                  (uint8_t)TUSB_ISO_EP_ATT_ADAPTIVE |
                  (uint8_t)TUSB_ISO_EP_ATT_DATA),
        /*_maxEPsize*/ getMaxEPSize(), /*_interval*/ 0x01)};
    append(buf, d11, sizeof(d11));

    /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.10.1.2) */
    // TUD_AUDIO_DESC_CS_AS_ISO_EP(/*_attr*/
    // AUDIO_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, /*_ctrl*/
    // AUDIO_CTRL_NONE, /*_lockdelayunit*/
    // AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC, /*_lockdelay*/ 0x0001),
    uint8_t d12[] = {TUD_AUDIO_DESC_CS_AS_ISO_EP(
        /*_attr*/ AUDIO_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK,
        /*_ctrl*/ AUDIO_CTRL_NONE,
        /*_lockdelayunit*/ AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC,
        /*_lockdelay*/ 0x0001)};
    append(buf, d12, sizeof(d12));

    // -- MIC ---
    /* Standard AS Interface Descriptor(4.9.1) */
    /* Interface 2, Alternate 0 - default alternate setting with 0 bandwidth */
    // TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/
    // (uint8_t)(ITF_NUM_AUDIO_STREAMING_MIC), /*_altset*/ 0x00, /*_nEPs*/ 0x00,
    // /*_stridx*/ 0x04),
    uint8_t d13[] = {TUD_AUDIO_DESC_STD_AS_INT(
        /*_itfnum*/ (uint8_t)(_itfnum_mic), /*_altset*/ 0x00, /*_nEPs*/ 0x00,
        /*_stridx*/ 0x04)};
    append(buf, d13, sizeof(d13));

    /* Standard AS Interface Descriptor(4.9.1) */
    /* Interface 2, Alternate 1 - alternate interface for data streaming */
    // TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/
    // (uint8_t)(ITF_NUM_AUDIO_STREAMING_MIC), /*_altset*/ 0x01, /*_nEPs*/ 0x01,
    // /*_stridx*/ 0x04),
    uint8_t d14[] = {TUD_AUDIO_DESC_STD_AS_INT(
        /*_itfnum*/ (uint8_t)(_itfnum_mic), /*_altset*/ 0x01, /*_nEPs*/ 0x01,
        /*_stridx*/ 0x04)};
    append(buf, d14, sizeof(d14));

    /* Class-Specific AS Interface Descriptor(4.9.2) */
    // TUD_AUDIO_DESC_CS_AS_INT(/*_termid*/ UAC2_ENTITY_MIC_OUTPUT_TERMINAL,
    // /*_ctrl*/ AUDIO_CTRL_NONE, /*_formattype*/ AUDIO_FORMAT_TYPE_I,
    // /*_formats*/ AUDIO_DATA_FORMAT_TYPE_I_PCM, /*_nchannelsphysical*/
    // CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX, /*_channelcfg*/
    // AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, /*_stridx*/ 0x00),
    uint8_t d15[] = {TUD_AUDIO_DESC_CS_AS_INT(
        /*_termid*/ UAC2_ENTITY_MIC_OUTPUT_TERMINAL, /*_ctrl*/ AUDIO_CTRL_NONE,
        /*_formattype*/ AUDIO_FORMAT_TYPE_I,
        /*_formats*/ AUDIO_DATA_FORMAT_TYPE_I_PCM,
        /*_nchannelsphysical*/ cfg.channels,
        /*_channelcfg*/ AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, /*_stridx*/ 0x00)};
    append(buf, d15, sizeof(d15));

    /* Type I Format Type Descriptor(2.3.1.6 - Audio Formats) */
    // TUD_AUDIO_DESC_TYPE_I_FORMAT(CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_TX,
    // CFG_TUD_AUDIO_FUNC_1_FORMAT_1_RESOLUTION_TX),
    uint8_t d16[] = {TUD_AUDIO_DESC_TYPE_I_FORMAT(
        (uint8_t)(cfg.bits_per_sample / 8), (uint8_t)cfg.bits_per_sample)};
    append(buf, d16, sizeof(d16));

    /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.10.1.1) */
    // TUD_AUDIO_DESC_STD_AS_ISO_EP(/*_ep*/ _epin, /*_attr*/ (uint8_t)
    // ((uint8_t)TUSB_XFER_ISOCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_ASYNCHRONOUS |
    // (uint8_t)TUSB_ISO_EP_ATT_DATA), /*_maxEPsize*/
    // TUD_AUDIO_EP_SIZE(CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE,
    // CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_TX,
    // CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX), /*_interval*/ 0x01),
    uint8_t d17[] = {TUD_AUDIO_DESC_STD_AS_ISO_EP(
        /*_ep*/ _ep_mic, /*_attr*/
        (uint8_t)((uint8_t)TUSB_XFER_ISOCHRONOUS |
                  (uint8_t)TUSB_ISO_EP_ATT_ASYNCHRONOUS |
                  (uint8_t)TUSB_ISO_EP_ATT_DATA),
        /*_maxEPsize*/ getMaxEPSize(), /*_interval*/ 0x01)};
    append(buf, d17, sizeof(d17));

    /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.10.1.2) */
    // TUD_AUDIO_DESC_CS_AS_ISO_EP(/*_attr*/
    // AUDIO_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, /*_ctrl*/
    // AUDIO_CTRL_NONE, /*_lockdelayunit*/
    // AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_UNDEFINED, /*_lockdelay*/
    // 0x0000),
    uint8_t d18[] = {TUD_AUDIO_DESC_CS_AS_ISO_EP(
        /*_attr*/ AUDIO_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK,
        /*_ctrl*/ AUDIO_CTRL_NONE,
        /*_lockdelayunit*/ AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_UNDEFINED,
        /*_lockdelay*/ 0x0000)};
    append(buf, d18, sizeof(d18));
  }
};
