#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

#include "AudioTools/Communication/USB/USBAudioBackend.h"
#include "AudioTools/Communication/USB/USBAudioConstants.h"
#include "AudioTools/Communication/USB/USBAudioHostBackend.h"
#include "AudioTools/Communication/USB/USBAudioStreamBase.h"

namespace audio_tools {

/**
 * @brief USB Audio Class (UAC1 and UAC2) host: reads PCM audio from an
 * attached USB microphone and/or writes PCM audio to an attached USB
 * speaker or headset, using the same API as the USB Audio device
 * (USBAudioStream).
 *
 * Directions (the RxTxMode is used the same way as for the device):
 *  - RX_MODE: readBytes() provides the audio of the attached microphone
 *    (isochronous IN endpoint of the device, config.enable_ep_in)
 *  - TX_MODE: write() sends audio to the attached speaker (isochronous OUT
 *    endpoint of the device, config.enable_ep_out)
 *  - RXTX_MODE: both, e.g. for a headset
 *
 * The requested sample_rate, channels and bits_per_sample are matched
 * against the alternate settings of the attached device. When the device
 * can not provide the requested format and config.host_apply_device_format
 * is true, the closest format is used, stored in the configuration and
 * reported to the subscribed listeners (setAudioInfo()). In RXTX_MODE the
 * format of the RX stream is reported; use audioInfoTx() for the TX format.
 *
 * setVolume() and setMute() are forwarded to the Feature Unit of the
 * attached device (all active directions); setAudioInfo() with a new
 * sample rate re-negotiates the streams. When config.volume_active is true,
 * the volume is in addition applied in software.
 *
 * This class implements the USB stack independent UAC logic: the stack
 * calls are delegated to a USBAudioHostBackend.
 *
 * @note Use the USBAudioHostStream type alias instead of this class.
 * @ingroup usb
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class USBAudioHostBase : public USBAudioStreamBase, public USBAudioHostEvents {
 public:
  USBAudioHostBase() = default;

  /// Constructor which provides the configuration at construction time
  USBAudioHostBase(USBAudioConfig cfg) { config_ = cfg; }

  /** @brief Returns a default configuration for the requested direction.
   *  RX_MODE reads from a USB microphone, TX_MODE writes to a USB speaker,
   *  RXTX_MODE does both (headset). */
  USBAudioConfig defaultConfig(RxTxMode mode = RXTX_MODE) {
    USBAudioConfig cfg;
    switch (mode) {
      case RX_MODE:
        cfg.enable_ep_in = true;
        cfg.enable_ep_out = false;
        break;
      case TX_MODE:
        cfg.enable_ep_in = false;
        cfg.enable_ep_out = true;
        break;
      case RXTX_MODE:
        cfg.enable_ep_in = true;
        cfg.enable_ep_out = true;
        break;
      default:
        break;
    }
    // feedback is handled automatically on the host side
    cfg.enable_feedback_ep = false;
    return cfg;
  }

  /** @brief Defines the audio format. Before begin() all fields can be
   *  changed; once started, a new sample rate is re-negotiated with the
   *  attached device. */
  void setAudioInfo(AudioInfo info) override {
    if (!isValidBitsPerSample(info.bits_per_sample)) {
      LOGE("Unsupported bits_per_sample: %d (must be 16, 24, or 32)",
           info.bits_per_sample);
      return;
    }
    if (!is_started_) {
      config_.copyFrom(info);
      return;
    }
    if (config_.channels != info.channels ||
        config_.bits_per_sample != info.bits_per_sample) {
      LOGE("Only the sample rate can be changed after begin()");
    }
    if (config_.sample_rate != info.sample_rate) {
      config_.sample_rate = info.sample_rate;
      // before the mount the new rate is used by the format negotiation
      if (mounted_) reconfigure_pending_ = true;
    }
  }

  /** @brief Apply a config and start the USB audio host.
   *  @return true on success. */
  bool begin(const USBAudioConfig& cfg) {
    if (!is_started_) {
      config_ = cfg;
    } else if (!configChanged(cfg)) {
      return true;
    } else {
      // only the sample rate can be changed at runtime
      AudioInfo info = audioInfo();
      info.sample_rate = cfg.sample_rate;
      setAudioInfo(info);
      return true;
    }
    return begin();
  }

  /** @brief Start the USB host stack with the current config. The attached
   *  device is opened as soon as it has been enumerated. */
  bool begin() {
    if (is_started_) return true;
    if (!isValidBitsPerSample(config_.bits_per_sample)) {
      LOGE("Unsupported bits_per_sample: %d (must be 16, 24, or 32)",
           config_.bits_per_sample);
      return false;
    }
    if (!config_.enable_ep_in && !config_.enable_ep_out) {
      LOGE("Neither RX (enable_ep_in) nor TX (enable_ep_out) is enabled");
      return false;
    }
    core = getActualCore();
    info = config_;
    rx_info_ = config_;
    tx_info_ = config_;
    setupVolumeState();
    buffer_bytes_ = packetSizeFor(config_);
    resizeBuffers();
    rx_primed_ = false;
    if (!beginUSB()) {
      LOGE("beginUSB failed");
      return false;
    }
    if (restart_pending_) {
      restart_pending_ = false;
      reconfiguring_ = false;
      next(Step::ClockRangeHeaderRx);
    }
    is_started_ = true;
    is_active_ = true;
    return true;
  }

  /** @brief Stop the streams and clear the buffers. The host stack keeps
   *  running, so the device is opened again with the next begin(). */
  void end() {
    is_started_ = false;
    bool was_streaming = isStreamingActive();
    if (rx_.active) backend().closeStream(daddr_, rx_.alt().ep.bEndpointAddress);
    if (tx_.active) {
      backend().closeStream(daddr_, tx_.alt().ep.bEndpointAddress);
      if (tx_.alt().has_fb)
        backend().closeStream(daddr_, tx_.alt().fb_ep.bEndpointAddress);
    }
    backend().end();
    rx_.active = false;
    tx_.active = false;
    mounted_ = false;
    step_ = Step::Idle;
    ctrl_busy_ = false;
    // a still attached device is reopened by the next begin()
    restart_pending_ = attached_ && configured_;
    bufferTx().reset();
    bufferRx().reset();
    if (was_streaming) notifyStreamingState();
  }

  /** @brief Returns true if an audio device is attached and configured. */
  bool mounted() override { return mounted_; }

  /** @brief Returns true if the speaker stream (used by write()) is open. */
  bool isStreamingActiveTx() const override { return tx_.active; }

  /** @brief Returns true if the microphone stream (used by readBytes()) is
   *  open. */
  bool isStreamingActiveRx() const override { return rx_.active; }

  /// True if the attached device provides a microphone (RX)
  bool hasMicrophone() const { return mounted_ && rx_.present(); }

  /// True if the attached device provides a speaker (TX)
  bool hasSpeaker() const { return mounted_ && tx_.present(); }

  /// Negotiated format of the microphone stream (readBytes())
  AudioInfo audioInfoRx() const { return rx_info_; }

  /// Negotiated format of the speaker stream (write())
  AudioInfo audioInfoTx() const { return tx_info_; }

  /// Vendor id of the attached device (0 if none)
  uint16_t vid() const { return vid_; }

  /// Product id of the attached device (0 if none)
  uint16_t pid() const { return pid_; }

  /// USB Audio Class version of the attached device: 1 or 2 (0 if none)
  uint8_t uacVersion() const { return uac_version_; }

  /// Number of isochronous OUT packets which were provided to the speaker
  uint32_t getTxXferCount() const { return tx_.xfer_count; }
  /// Number of isochronous IN packets which were received from the
  /// microphone
  uint32_t getRxXferCount() const { return rx_.xfer_count; }
  /// Total bytes received from the microphone
  uint32_t getRxTotalBytes() const { return rx_.total_bytes; }
  /// Bytes discarded because bufferRx() was full
  uint32_t getRxDroppedBytes() const { return rx_.dropped_bytes; }
  /// Bytes of silence which were sent because bufferTx() was empty
  uint32_t getTxUnderflowBytes() const { return tx_.dropped_bytes; }

  /// One isochronous packet (1 ms at full speed) of the largest stream in
  /// bytes
  uint16_t audioPacketSize() const { return buffer_bytes_; }

 protected:
  static constexpr uint8_t kMaxRates = 16;
  static constexpr uint8_t kMaxChannels = 8;
  static constexpr uint16_t kCtrlBufSize = 2 + 12 * 24;

  /// One alternate setting of an audio streaming interface
  struct AltSetting {
    uint8_t itf = 0;
    uint8_t alt = 0;
    uint8_t terminal_link = 0;
    bool is_pcm = true;
    uint8_t channels = 0;
    uint8_t subslot = 0;  // bytes per sample container
    uint8_t bits = 0;     // bit resolution
    bool has_ep = false;
    UsbEndpointDescriptorView ep;
    bool has_fb = false;
    UsbEndpointDescriptorView fb_ep;
    // UAC1: rates of the format descriptor; UAC2: from the clock source
    bool continuous = false;
    uint32_t rates[kMaxRates] = {};
    uint8_t rate_count = 0;
  };

  /// Range of supported sample rates (UAC2 clock source GET_RANGE)
  struct RateRange {
    uint32_t min = 0;
    uint32_t max = 0;
    uint32_t res = 0;
  };

  /// State of one direction (microphone or speaker)
  struct Stream {
    std::vector<AltSetting> alts;
    int selected = -1;
    uint32_t sample_rate = 0;
    uint8_t fu_id = 0;
    uint8_t clock_id = 0;
    // Feature Unit capabilities: bit n = control available for channel n
    uint16_t fu_mute_mask = 0;
    uint16_t fu_volume_mask = 0;
    int16_t vol_min = kVolumeMinDb256;
    int16_t vol_max = 0;
    bool active = false;
    // TX pacing: exact accumulator (sample_rate per packets_per_sec) for the
    // nominal rate, 16.16 samples per packet for explicit feedback
    uint32_t acc = 0;
    uint32_t acc_fb = 0;
    uint32_t nominal = 0;  // 16.16 samples per packet
    volatile uint32_t feedback = 0;
    uint16_t packets_per_sec = 1000;
    bool primed = false;
    // statistics
    uint32_t xfer_count = 0;
    uint32_t total_bytes = 0;
    uint32_t dropped_bytes = 0;

    bool present() const { return !alts.empty(); }
    AltSetting& alt() { return alts[selected < 0 ? 0 : selected]; }
    const AltSetting& alt() const { return alts[selected < 0 ? 0 : selected]; }
    uint16_t frameBytes() const {
      return selected < 0 ? 0
                          : (uint16_t)alt().channels * (uint16_t)alt().subslot;
    }
    void reset() {
      alts.clear();
      selected = -1;
      sample_rate = 0;
      fu_id = clock_id = 0;
      fu_mute_mask = fu_volume_mask = 0;
      vol_min = kVolumeMinDb256;
      vol_max = 0;
      active = false;
      acc = acc_fb = nominal = 0;
      feedback = 0;
      primed = false;
    }
  };

  /// Audio Control entity (terminal, unit or clock) for the topology walk
  struct Entity {
    uint8_t id = 0;
    uint8_t subtype = 0;
    uint16_t terminal_type = 0;
    uint8_t clock_source = 0;  // bCSourceID (UAC2 terminals)
    uint8_t sources[4] = {};
    uint8_t source_count = 0;
    uint16_t mute_mask = 0;
    uint16_t volume_mask = 0;
  };

  /// Steps of the (re)configuration state machine
  enum class Step : uint8_t {
    Idle,
    StopRx,
    StopTx,
    ClockRangeHeaderRx,
    ClockRangeRx,
    ClockRangeHeaderTx,
    ClockRangeTx,
    Select,
    SetClockRx,
    SetInterfaceRx,
    OpenRx,
    SetEpRateRx,
    VolumeRangeRx,
    VolumeMaxRx,
    SetClockTx,
    SetInterfaceTx,
    OpenTx,
    SetEpRateTx,
    VolumeRangeTx,
    VolumeMaxTx,
    Done
  };

  uint8_t daddr_ = 0;
  uint16_t vid_ = 0;
  uint16_t pid_ = 0;
  uint8_t uac_version_ = 0;
  uint8_t itf_num_ac_ = 0;
  volatile bool attached_ = false;
  volatile bool mounted_ = false;
  volatile bool configured_ = false;
  Stream rx_;
  Stream tx_;
  std::vector<Entity> entities_;
  std::vector<RateRange> clock_ranges_rx_;
  std::vector<RateRange> clock_ranges_tx_;
  AudioInfo rx_info_;
  AudioInfo tx_info_;
  uint16_t buffer_bytes_ = 0;

  // state machine
  Step step_ = Step::Idle;
  bool ctrl_busy_ = false;
  bool ctrl_ok_ = false;
  uint16_t ctrl_len_ = 0;
  bool step_done_ = false;
  bool in_run_ = false;
  bool run_again_ = false;
  bool reconfiguring_ = false;
  uint32_t ctrl_start_ms_ = 0;
  uint8_t ctrl_buf_[kCtrlBufSize] = {};
  UsbSetupPacket setup_;
  volatile bool reconfigure_pending_ = false;
  bool restart_pending_ = false;
  // runtime volume / mute requests (one flag per channel)
  std::vector<uint8_t> volume_pending_;
  std::vector<uint8_t> mute_pending_;
  uint8_t pending_channel_ = 0;
  bool pending_is_volume_ = false;
  Stream* pending_stream_ = nullptr;

  /** @brief Returns the backend used for all USB host stack calls. */
  virtual USBAudioHostBackend& backend() = 0;

  bool beginUSB() override { return backend().begin(this, config_); }

  /// Pump the USB stack and process queued requests: called by the
  /// platform specific serviceUSB() or USB task.
  void processUSB() {
    backend().task();
    poll();
  }

  // ── USBAudioHostEvents ───────────────────────────────────────────────────

  uint16_t onAttach(uint8_t daddr, uint16_t vid, uint16_t pid,
                    const uint8_t* desc, uint16_t len) override {
    if (attached_) return 0;  // we support only one audio function
    if (len < 9 || descType(desc) != kUsbDescTypeInterface) return 0;
    UsbInterfaceDescriptorView ac = decodeInterface(desc);
    if (ac.bInterfaceClass != kUsbClassAudio ||
        ac.bInterfaceSubClass != kAudioSubclassControl)
      return 0;
    if (config_.host_vid != 0 && config_.host_vid != vid) return 0;
    if (config_.host_pid != 0 && config_.host_pid != pid) return 0;

    resetDevice();
    uint16_t consumed = parseConfiguration(desc, len);
    if (!rx_.present() && !tx_.present()) {
      LOGW("USB audio device %04x:%04x has no PCM streaming interface", vid,
           pid);
      resetDevice();
      return consumed;
    }
    daddr_ = daddr;
    vid_ = vid;
    pid_ = pid;
    attached_ = true;
    LOGI("USB audio device %04x:%04x (UAC%d) attached: mic=%d speaker=%d",
         vid, pid, uac_version_, rx_.present(), tx_.present());
    return consumed;
  }

  void onConfigured(uint8_t daddr) override {
    if (!attached_ || daddr != daddr_) return;
    configured_ = true;
    // the sequence is started from poll() so that we never issue a control
    // request while the stack is still configuring other interfaces
    reconfiguring_ = false;
    next(Step::ClockRangeHeaderRx);
  }

  void onDetach(uint8_t daddr) override {
    if (!attached_ || daddr != daddr_) return;
    bool was_streaming = isStreamingActive();
    LOGI("USB audio device detached");
    resetDevice();
    if (was_streaming) notifyStreamingState();
  }

  void onControlComplete(bool ok, uint16_t len) override {
    if (!ctrl_busy_) return;  // late completion after a timeout
    ctrl_busy_ = false;
    ctrl_ok_ = ok;
    ctrl_len_ = len;
    step_done_ = true;
    if (step_ != Step::Idle) {
      run();
    } else {
      finishRuntimeRequest();
    }
  }

  void onIsoIn(UsbHostStreamRole role, const uint8_t* data,
               uint16_t len) override {
    if (role == UsbHostStreamRole::Feedback) {
      onFeedback(data, len);
      return;
    }
    if (!rx_.active) return;
    rx_.xfer_count++;
    if (len == 0) return;
    rx_.total_bytes += len;
    int written = bufferRx().writeArray(data, len);
    if (written < len) rx_.dropped_bytes += (len - written);
  }

  uint16_t onIsoOut(uint8_t* data, uint16_t max) override {
    if (!tx_.active) return 0;
    tx_.xfer_count++;
    uint16_t frame = tx_.frameBytes();
    if (frame == 0) return 0;
    uint32_t samples = 0;
    uint32_t fb = tx_.feedback;
    if (fb != 0) {
      tx_.acc_fb += fb;
      samples = tx_.acc_fb >> 16;
      tx_.acc_fb &= 0xFFFF;
    } else {
      tx_.acc += tx_.sample_rate;
      samples = tx_.acc / tx_.packets_per_sec;
      tx_.acc -= samples * tx_.packets_per_sec;
    }
    uint16_t bytes = (uint16_t)(samples * frame);
    uint16_t max_aligned = (uint16_t)(max - (max % frame));
    if (bytes > max_aligned) bytes = max_aligned;

    // one-shot prime: wait until half of the buffer is filled
    if (!tx_.primed) {
      if (bufferTx().levelPercent() < 50.0f) {
        memset(data, 0, bytes);
        return bytes;
      }
      tx_.primed = true;
    }

    int avail = bufferTx().available();
    uint16_t to_read = bytes;
    if (avail < to_read) to_read = (uint16_t)(avail - (avail % frame));
    int read = to_read > 0 ? bufferTx().readArray(data, to_read) : 0;
    if (read < bytes) {
      memset(data + read, 0, bytes - read);
      tx_.dropped_bytes += (bytes - read);
    }
    return bytes;
  }

  // ── Volume / Mute propagation ────────────────────────────────────────────

  void onVolumeChanged(uint8_t channel) override {
    if (channel < volume_pending_.size()) volume_pending_[channel] = 1;
  }

  void onMuteChanged(uint8_t channel) override {
    if (channel < mute_pending_.size()) mute_pending_[channel] = 1;
  }

  // ── Buffer sizing ────────────────────────────────────────────────────────

  /// Bytes of one 1 ms packet for the indicated format (+1 sample margin)
  static uint16_t packetSizeFor(const AudioInfo& info) {
    uint32_t samples = (info.sample_rate + 999) / 1000 + 1;
    return (uint16_t)(samples * info.channels * (info.bits_per_sample / 8));
  }

  /// Make sure that the buffers can hold fifo_packets packets of the
  /// negotiated format
  void updateBufferSize() {
    uint16_t needed = 0;
    if (rx_.selected >= 0) needed = packetSizeFor(rx_info_);
    if (tx_.selected >= 0) {
      uint16_t tx = packetSizeFor(tx_info_);
      if (tx > needed) needed = tx;
    }
    if (needed > buffer_bytes_) {
      buffer_bytes_ = needed;
      resizeBuffers();
    }
  }

  // ── Descriptor parsing ───────────────────────────────────────────────────

  Entity* findEntity(uint8_t id) {
    for (auto& e : entities_) {
      if (e.id == id) return &e;
    }
    return nullptr;
  }

  /// Parses the audio function which starts with the AC interface at desc.
  /// Returns the number of bytes which belong to the audio function.
  uint16_t parseConfiguration(const uint8_t* desc, uint16_t len) {
    const uint8_t* p = desc;
    const uint8_t* end = desc + len;
    itf_num_ac_ = decodeInterface(p).bInterfaceNumber;
    uac_version_ = 0;
    bool in_ac = true;
    AltSetting* cur = nullptr;
    AltSetting pending;
    bool has_pending = false;
    std::vector<AltSetting> all;

    auto flush = [&]() {
      if (has_pending && pending.has_ep && pending.is_pcm) all.push_back(pending);
      has_pending = false;
      cur = nullptr;
    };

    p = descNext(p);
    while (p + 2 <= end && p[0] >= 2 && p + p[0] <= end) {
      uint8_t type = descType(p);
      if (type == kUsbDescTypeIad) break;  // next function
      if (type == kUsbDescTypeInterface && p[0] >= 9) {
        UsbInterfaceDescriptorView itf = decodeInterface(p);
        if (itf.bInterfaceClass != kUsbClassAudio) break;
        if (itf.bInterfaceSubClass == kAudioSubclassControl) {
          if (itf.bInterfaceNumber != itf_num_ac_) break;  // next function
        } else if (itf.bInterfaceSubClass == kAudioSubclassStreaming) {
          in_ac = false;
          flush();
          if (itf.bNumEndpoints > 0) {
            pending = AltSetting();
            pending.itf = itf.bInterfaceNumber;
            pending.alt = itf.bAlternateSetting;
            has_pending = true;
            cur = &pending;
          }
        } else {
          break;  // e.g. MIDI streaming: not handled by us
        }
      } else if (type == kUsbDescTypeCsInterface) {
        if (in_ac) {
          parseAcDescriptor(p);
        } else if (cur) {
          parseAsDescriptor(*cur, p);
        }
      } else if (type == kUsbDescTypeEndpoint && cur && p[0] >= 7) {
        UsbEndpointDescriptorView ep = decodeEndpoint(p);
        if (ep.xferType == UsbXferType::Isochronous) {
          bool is_feedback = ep.usage == 1 ||
                             (cur->has_ep && ep.direction() == UsbDir::In &&
                              cur->ep.direction() == UsbDir::Out);
          if (is_feedback) {
            cur->has_fb = true;
            cur->fb_ep = ep;
          } else if (!cur->has_ep) {
            cur->has_ep = true;
            cur->ep = ep;
          }
        }
      }
      p = descNext(p);
    }
    flush();

    for (auto& a : all) {
      if (!isValidBitsPerSample(a.subslot * 8) || a.channels == 0) {
        LOGI("Ignoring alt %d of itf %d: %d channels, %d bytes/sample", a.alt,
             a.itf, a.channels, a.subslot);
        continue;
      }
      bool is_rx = a.ep.direction() == UsbDir::In;
      if (is_rx && !config_.enable_ep_in) continue;
      if (!is_rx && !config_.enable_ep_out) continue;
      Stream& s = is_rx ? rx_ : tx_;
      // only one streaming interface per direction
      if (s.present() && s.alts[0].itf != a.itf) continue;
      s.alts.push_back(a);
    }
    resolveTopology(rx_, true);
    resolveTopology(tx_, false);
    return (uint16_t)(p - desc);
  }

  /// Audio Control interface: header, terminals, units and clocks
  void parseAcDescriptor(const uint8_t* p) {
    uint8_t subtype = descSubtype(p);
    uint8_t n = p[0];
    if (subtype == kAudioCsAcHeader && n >= 5) {
      uac_version_ = descU16(p + 3) >= 0x0200 ? 2 : 1;
      return;
    }
    if (n < 4) return;
    bool v2 = uac_version_ == 2;
    Entity e;
    e.id = p[3];
    e.subtype = subtype;
    auto addSources = [&](uint8_t count_pos) {
      if (n <= count_pos) return;
      uint8_t cnt = p[count_pos];
      for (uint8_t j = 0; j < cnt && e.source_count < 4 &&
                          (uint16_t)(count_pos + 1 + j) < n;
           j++) {
        e.sources[e.source_count++] = p[count_pos + 1 + j];
      }
    };
    switch (subtype) {
      case 0x02:  // input terminal
        if (n >= 8) {
          e.terminal_type = descU16(p + 4);
          if (v2) e.clock_source = p[7];
        }
        break;
      case 0x03:  // output terminal
        if (n >= 8) {
          e.terminal_type = descU16(p + 4);
          e.sources[0] = p[7];
          e.source_count = 1;
          if (v2 && n >= 9) e.clock_source = p[8];
        }
        break;
      case 0x04:  // mixer unit
      case 0x05:  // selector unit
        addSources(4);
        break;
      case kAudioCsAcFeatureUnit:
        if (n >= 6) {
          e.sources[0] = p[4];
          e.source_count = 1;
          parseFeatureUnitControls(e, p, v2);
        }
        break;
      case 0x07:  // UAC1 processing unit / UAC2 effect unit
        if (v2) {
          if (n >= 7) {
            e.sources[0] = p[6];
            e.source_count = 1;
          }
        } else {
          addSources(6);
        }
        break;
      case 0x08:  // UAC1 extension unit / UAC2 processing unit
      case 0x09:  // UAC2 extension unit
        addSources(6);
        break;
      case kAudioCsAcClockSource:  // UAC2 clock source
        break;
      case 0x0B:  // UAC2 clock selector
        addSources(4);
        break;
      case 0x0C:  // UAC2 clock multiplier
        if (n >= 5) {
          e.sources[0] = p[4];
          e.source_count = 1;
        }
        break;
      default:
        return;
    }
    entities_.push_back(e);
  }

  /// Determines which channels provide a host programmable mute / volume
  void parseFeatureUnitControls(Entity& e, const uint8_t* p, bool v2) {
    uint8_t n = p[0];
    if (v2) {
      // 4 bytes per channel starting at 5, 2 bits per control
      for (uint8_t ch = 0; ch <= kMaxChannels && 5 + ch * 4 + 4 <= n - 1;
           ch++) {
        uint32_t ctrl = descU32(p + 5 + ch * 4);
        if ((ctrl & 0x03) == 0x03) e.mute_mask |= (1u << ch);
        if (((ctrl >> 2) & 0x03) == 0x03) e.volume_mask |= (1u << ch);
      }
    } else {
      uint8_t size = p[5];
      if (size == 0 || n < 7) return;
      for (uint8_t ch = 0; ch <= kMaxChannels && 6 + (ch + 1) * size <= n - 1;
           ch++) {
        uint8_t ctrl = p[6 + ch * size];
        if (ctrl & 0x01) e.mute_mask |= (1u << ch);
        if (ctrl & 0x02) e.volume_mask |= (1u << ch);
      }
    }
  }

  /// Audio Streaming interface: AS_GENERAL and FORMAT_TYPE descriptors
  void parseAsDescriptor(AltSetting& a, const uint8_t* p) {
    uint8_t subtype = descSubtype(p);
    uint8_t n = p[0];
    if (uac_version_ == 2) {
      if (subtype == kAudioCsAsGeneral && n >= 11) {
        a.terminal_link = p[3];
        // bFormatType == 1 and bmFormats bit 0 (PCM)
        a.is_pcm = p[5] == kAudioFormatTypeI && (descU32(p + 6) & 0x01);
        a.channels = p[10];
      } else if (subtype == kAudioCsAsFormatType && n >= 6) {
        if (p[3] != kAudioFormatTypeI) a.is_pcm = false;
        a.subslot = p[4];
        a.bits = p[5];
      }
    } else {
      if (subtype == kAudioCsAsGeneral && n >= 7) {
        a.terminal_link = p[3];
        a.is_pcm = descU16(p + 5) == 0x0001;  // wFormatTag: PCM
      } else if (subtype == kAudioCsAsFormatType && n >= 8) {
        if (p[3] != kAudioFormatTypeI) {
          a.is_pcm = false;
          return;
        }
        a.channels = p[4];
        a.subslot = p[5];
        a.bits = p[6];
        uint8_t freq_type = p[7];
        if (freq_type == 0 && n >= 14) {
          a.continuous = true;
          a.rates[0] = descU24(p + 8);
          a.rates[1] = descU24(p + 11);
          a.rate_count = 2;
        } else {
          for (uint8_t j = 0; j < freq_type && j < kMaxRates &&
                              8 + (j + 1) * 3 <= n;
               j++) {
            a.rates[a.rate_count++] = descU24(p + 8 + j * 3);
          }
        }
      }
    }
  }

  static uint32_t descU24(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
  }

  /// Finds the Feature Unit and the Clock Source of a stream by following
  /// the entity links of its USB streaming terminal.
  void resolveTopology(Stream& s, bool is_rx) {
    if (!s.present()) return;
    uint8_t link = s.alts[0].terminal_link;
    Entity* term = findEntity(link);
    if (term == nullptr) return;

    if (uac_version_ == 2) s.clock_id = resolveClock(term->clock_source);

    Entity* fu = nullptr;
    if (is_rx) {
      // microphone: walk upstream from the USB streaming output terminal
      Entity* e = term;
      for (int depth = 0; depth < 8 && e && e->source_count > 0; depth++) {
        e = findEntity(e->sources[0]);
        if (e && e->subtype == kAudioCsAcFeatureUnit) {
          fu = e;
          break;
        }
      }
    } else {
      // speaker: walk downstream from the USB streaming input terminal
      uint8_t id = term->id;
      for (int depth = 0; depth < 8 && fu == nullptr; depth++) {
        Entity* next = nullptr;
        for (auto& e : entities_) {
          if (e.subtype == kAudioCsAcClockSource || e.subtype == 0x0B ||
              e.subtype == 0x0C)
            continue;
          for (uint8_t j = 0; j < e.source_count; j++) {
            if (e.sources[j] == id) next = &e;
          }
          if (next) break;
        }
        if (next == nullptr) break;
        if (next->subtype == kAudioCsAcFeatureUnit) fu = next;
        id = next->id;
      }
    }
    if (fu) {
      s.fu_id = fu->id;
      s.fu_mute_mask = fu->mute_mask;
      s.fu_volume_mask = fu->volume_mask;
    }
    LOGI("%s: itf %d, %d alt settings, feature unit %d, clock %d",
         is_rx ? "mic" : "speaker", s.alts[0].itf, (int)s.alts.size(),
         s.fu_id, s.clock_id);
  }

  /// Follow clock selectors and multipliers to the clock source
  uint8_t resolveClock(uint8_t id) {
    for (int depth = 0; depth < 4 && id != 0; depth++) {
      Entity* e = findEntity(id);
      if (e == nullptr) return id;
      if (e->subtype == kAudioCsAcClockSource) return id;
      if (e->source_count == 0) return 0;
      id = e->sources[0];
    }
    return id;
  }

  // ── Format negotiation ───────────────────────────────────────────────────

  /// True if the alternate setting supports the sample rate
  bool supportsRate(const AltSetting& a,
                    const std::vector<RateRange>& clock, uint32_t rate) {
    if (!fitsBandwidth(a, rate)) return false;
    if (uac_version_ == 2) {
      if (clock.empty()) return true;  // unknown: assume that it works
      for (auto& r : clock) {
        if (rate < r.min || rate > r.max) continue;
        if (r.res == 0 || (rate - r.min) % r.res == 0) return true;
      }
      return false;
    }
    if (a.rate_count == 0) return true;
    if (a.continuous) return rate >= a.rates[0] && rate <= a.rates[1];
    for (uint8_t j = 0; j < a.rate_count; j++) {
      if (a.rates[j] == rate) return true;
    }
    return false;
  }

  bool fitsBandwidth(const AltSetting& a, uint32_t rate) {
    uint32_t pps = packetsPerSecond(a);
    uint32_t samples = (rate + pps - 1) / pps;
    return samples * a.channels * a.subslot <= a.ep.packetSize();
  }

  uint16_t packetsPerSecond(const AltSetting& a) {
    uint8_t interval = a.ep.bInterval;
    if (uac_version_ == 1 || interval == 0) interval = 1;
    if (interval > 4) interval = 4;
    uint16_t base = backend().speed() == UsbSpeed::High ? 8000 : 1000;
    return (uint16_t)(base >> (interval - 1));
  }

  /// Supported sample rate which is closest to the wanted rate (0 if none)
  uint32_t closestRate(const AltSetting& a,
                       const std::vector<RateRange>& clock,
                       uint32_t wanted) {
    if (supportsRate(a, clock, wanted)) return wanted;
    static const uint32_t common[] = {48000, 44100, 32000, 24000, 22050,
                                      16000, 11025, 8000,  96000, 88200};
    uint32_t best = 0;
    uint32_t best_diff = UINT32_MAX;
    auto consider = [&](uint32_t r) {
      if (r == 0 || !supportsRate(a, clock, r)) return;
      uint32_t diff = r > wanted ? r - wanted : wanted - r;
      if (diff < best_diff) {
        best = r;
        best_diff = diff;
      }
    };
    if (uac_version_ == 2) {
      for (auto& r : clock) {
        consider(r.min);
        consider(r.max);
      }
    } else {
      for (uint8_t j = 0; j < a.rate_count; j++) consider(a.rates[j]);
    }
    for (auto r : common) consider(r);
    return best;
  }

  /// Selects the alternate setting which matches the requested format best
  bool selectAlt(Stream& s, const std::vector<RateRange>& clock,
                 AudioInfo& negotiated) {
    s.selected = -1;
    if (!s.present()) return false;
    int best = -1;
    int best_score = -1;
    uint32_t best_rate = 0;
    for (int j = 0; j < (int)s.alts.size(); j++) {
      const AltSetting& a = s.alts[j];
      uint32_t rate = closestRate(a, clock, config_.sample_rate);
      if (rate == 0) continue;
      int score = 0;
      if (rate == config_.sample_rate) score += 4;
      if (a.channels == config_.channels) score += 2;
      if (a.subslot * 8 == config_.bits_per_sample) score += 1;
      // prefer the smaller packet size on a tie
      if (score > best_score ||
          (score == best_score &&
           a.ep.packetSize() < s.alts[best].ep.packetSize())) {
        best = j;
        best_score = score;
        best_rate = rate;
      }
    }
    if (best < 0) return false;
    if (best_score != 7 && !config_.host_apply_device_format) {
      LOGE("The attached device does not support %d Hz, %d channels, %d bits",
           (int)config_.sample_rate, config_.channels, config_.bits_per_sample);
      return false;
    }
    s.selected = best;
    s.sample_rate = best_rate;
    negotiated.sample_rate = best_rate;
    negotiated.channels = s.alts[best].channels;
    negotiated.bits_per_sample = s.alts[best].subslot * 8;
    LOGI("Selected itf %d alt %d: %d Hz, %d channels, %d bits",
         s.alts[best].itf, s.alts[best].alt, (int)best_rate,
         negotiated.channels, negotiated.bits_per_sample);
    return true;
  }

  // ── Control request helpers ──────────────────────────────────────────────

  bool issueControl(uint8_t request_type, uint8_t request, uint16_t value,
                    uint16_t index, uint16_t length) {
    setup_.bmRequestType = request_type;
    setup_.bRequest = request;
    setup_.wValue = value;
    setup_.wIndex = index;
    setup_.wLength = length;
    ctrl_busy_ = true;
    ctrl_start_ms_ = backend().millis();
    if (!backend().control(daddr_, setup_, ctrl_buf_)) {
      ctrl_busy_ = false;
      return false;
    }
    return true;
  }

  bool issueSetInterface(uint8_t itf, uint8_t alt) {
    ctrl_busy_ = true;
    ctrl_start_ms_ = backend().millis();
    if (!backend().setInterface(daddr_, itf, alt)) {
      ctrl_busy_ = false;
      return false;
    }
    return true;
  }

  // class request to an interface entity, host to device
  static constexpr uint8_t kReqOutItf = 0x21;
  // class request to an interface entity, device to host
  static constexpr uint8_t kReqInItf = 0xA1;
  // class request to an endpoint, host to device
  static constexpr uint8_t kReqOutEp = 0x22;

  bool requestClockRange(Stream& s, uint16_t len) {
    return issueControl(kReqInItf, kAudioCsReqRange,
                        (uint16_t)(kAudioCsCtrlSamFreq << 8),
                        (uint16_t)((s.clock_id << 8) | itf_num_ac_), len);
  }

  bool requestSetClock(Stream& s) {
    uint32_t rate = s.sample_rate;
    for (int j = 0; j < 4; j++) ctrl_buf_[j] = (uint8_t)(rate >> (8 * j));
    return issueControl(kReqOutItf, kAudioCsReqCur,
                        (uint16_t)(kAudioCsCtrlSamFreq << 8),
                        (uint16_t)((s.clock_id << 8) | itf_num_ac_), 4);
  }

  bool requestSetEpRate(Stream& s) {
    uint32_t rate = s.sample_rate;
    for (int j = 0; j < 3; j++) ctrl_buf_[j] = (uint8_t)(rate >> (8 * j));
    return issueControl(kReqOutEp, kAudioCsReqCur,
                        (uint16_t)(kAudio1EpCtrlSamplingFreq << 8),
                        s.alt().ep.bEndpointAddress, 3);
  }

  /// Channel used to determine the volume range
  static uint8_t volumeRangeChannel(const Stream& s) {
    if (s.fu_volume_mask & 0x01) return 0;
    for (uint8_t ch = 1; ch <= kMaxChannels; ch++) {
      if (s.fu_volume_mask & (1u << ch)) return ch;
    }
    return 0;
  }

  bool requestVolumeRange(Stream& s, bool max) {
    uint8_t ch = volumeRangeChannel(s);
    uint16_t value = (uint16_t)((AUDIO_FU_CTRL_VOLUME << 8) | ch);
    uint16_t index = (uint16_t)((s.fu_id << 8) | itf_num_ac_);
    if (uac_version_ == 2) {
      return issueControl(kReqInItf, kAudioCsReqRange, value, index, 8);
    }
    return issueControl(kReqInItf, max ? kAudio1ReqGetMax : kAudio1ReqGetMin,
                        value, index, 2);
  }

  bool requestFeature(Stream& s, uint8_t selector, uint8_t ch, int16_t value) {
    uint16_t len = 1;
    if (selector == AUDIO_FU_CTRL_VOLUME) {
      ctrl_buf_[0] = (uint8_t)(value & 0xFF);
      ctrl_buf_[1] = (uint8_t)((uint16_t)value >> 8);
      len = 2;
    } else {
      ctrl_buf_[0] = value ? 1 : 0;
    }
    return issueControl(kReqOutItf, kAudioCsReqCur,
                        (uint16_t)((selector << 8) | ch),
                        (uint16_t)((s.fu_id << 8) | itf_num_ac_), len);
  }

  /// Parses a UAC2 GET_RANGE (layout 3: 32 bit) response for the clock
  void parseClockRange(std::vector<RateRange>& ranges) {
    ranges.clear();
    if (ctrl_len_ < 2) return;
    uint16_t count = descU16(ctrl_buf_);
    for (uint16_t j = 0; j < count && 2 + (j + 1) * 12 <= ctrl_len_; j++) {
      RateRange r;
      r.min = descU32(ctrl_buf_ + 2 + j * 12);
      r.max = descU32(ctrl_buf_ + 6 + j * 12);
      r.res = descU32(ctrl_buf_ + 10 + j * 12);
      ranges.push_back(r);
    }
  }

  /// Linear mapping of 0.0..1.0 to the volume range of the device
  static int16_t toDeviceVolume(const Stream& s, float vol) {
    if (vol <= 0.0f) return s.vol_min;
    if (vol >= 1.0f) return s.vol_max;
    return (int16_t)(s.vol_min + vol * (float)(s.vol_max - s.vol_min));
  }

  // ── State machine ────────────────────────────────────────────────────────

  /// Called regularly from the USB stack context
  void poll() {
    if (!is_started_ || !attached_ || !configured_) return;
    if (ctrl_busy_) {
      if (backend().millis() - ctrl_start_ms_ > config_.host_ctrl_timeout_ms) {
        LOGW("USB audio control request timed out");
        onControlComplete(false, 0);
      }
      return;
    }
    if (step_ != Step::Idle) {
      run();
      return;
    }
    if (!mounted_) return;
    if (reconfigure_pending_) {
      reconfigure_pending_ = false;
      reconfiguring_ = true;
      next(Step::StopRx);
      run();
      return;
    }
    startRuntimeRequest();
  }

  /// Runs the state machine until a control request is in flight
  void run() {
    if (in_run_) {
      run_again_ = true;
      return;
    }
    in_run_ = true;
    do {
      run_again_ = false;
      while (step_ != Step::Idle && !ctrl_busy_ && attached_) {
        if (!executeStep()) break;  // retry in the next poll()
      }
    } while (run_again_);
    in_run_ = false;
  }

  /// Advance to the next step
  void next(Step s) {
    step_ = s;
    step_done_ = false;
  }

  /// Executes the current step. Returns false if the step could not be
  /// started and needs to be retried later.
  bool executeStep() {
    // Steps which issue a control request are entered twice: first to send
    // the request (step_done_ == false), then with the result.
    switch (step_) {
      case Step::StopRx:
        return stopStream(rx_, Step::StopTx);
      case Step::StopTx:
        return stopStream(tx_, Step::ClockRangeHeaderRx);

      case Step::ClockRangeHeaderRx:
        return clockRangeHeader(rx_, clock_ranges_rx_, Step::ClockRangeRx,
                                Step::ClockRangeHeaderTx);
      case Step::ClockRangeRx:
        return clockRange(rx_, clock_ranges_rx_, Step::ClockRangeHeaderTx);
      case Step::ClockRangeHeaderTx:
        if (tx_.present() && rx_.present() && tx_.clock_id == rx_.clock_id &&
            uac_version_ == 2) {
          clock_ranges_tx_ = clock_ranges_rx_;
          next(Step::Select);
          return true;
        }
        return clockRangeHeader(tx_, clock_ranges_tx_, Step::ClockRangeTx,
                                Step::Select);
      case Step::ClockRangeTx:
        return clockRange(tx_, clock_ranges_tx_, Step::Select);

      case Step::Select: {
        bool rx_ok = selectAlt(rx_, clock_ranges_rx_, rx_info_);
        bool tx_ok = selectAlt(tx_, clock_ranges_tx_, tx_info_);
        if (!rx_ok && !tx_ok) {
          LOGE("No usable audio stream on the attached device");
          next(Step::Idle);
          return true;
        }
        if (rx_.present() && !rx_ok) rx_.alts.clear();
        if (tx_.present() && !tx_ok) tx_.alts.clear();
        // a common clock can only run at one rate
        if (uac_version_ == 2 && rx_ok && tx_ok &&
            rx_.clock_id == tx_.clock_id && rx_.sample_rate != tx_.sample_rate) {
          LOGW("Shared clock: using %d Hz for both streams",
               (int)rx_.sample_rate);
          tx_.sample_rate = rx_.sample_rate;
          tx_info_.sample_rate = rx_.sample_rate;
        }
        updateBufferSize();
        next(Step::SetClockRx);
        return true;
      }

      case Step::SetClockRx:
        return setClock(rx_, Step::SetInterfaceRx);
      case Step::SetInterfaceRx:
        return setInterface(rx_, Step::OpenRx, Step::SetClockTx);
      case Step::OpenRx:
        openStream(rx_, true);
        next(Step::SetEpRateRx);
        return true;
      case Step::SetEpRateRx:
        return setEpRate(rx_, Step::VolumeRangeRx);
      case Step::VolumeRangeRx:
        return volumeRange(rx_, false, Step::VolumeMaxRx, Step::SetClockTx);
      case Step::VolumeMaxRx:
        return volumeRange(rx_, true, Step::SetClockTx, Step::SetClockTx);

      case Step::SetClockTx:
        if (tx_.selected >= 0 && rx_.selected >= 0 &&
            tx_.clock_id == rx_.clock_id) {
          next(Step::SetInterfaceTx);  // shared clock was already set
          return true;
        }
        return setClock(tx_, Step::SetInterfaceTx);
      case Step::SetInterfaceTx:
        return setInterface(tx_, Step::OpenTx, Step::Done);
      case Step::OpenTx:
        openStream(tx_, false);
        next(Step::SetEpRateTx);
        return true;
      case Step::SetEpRateTx:
        return setEpRate(tx_, Step::VolumeRangeTx);
      case Step::VolumeRangeTx:
        return volumeRange(tx_, false, Step::VolumeMaxTx, Step::Done);
      case Step::VolumeMaxTx:
        return volumeRange(tx_, true, Step::Done, Step::Done);

      case Step::Done:
        finishMount();
        next(Step::Idle);
        return true;

      default:
        next(Step::Idle);
        return true;
    }
  }

  /// Stop streaming before a reconfiguration: select alt 0
  bool stopStream(Stream& s, Step following) {
    if (s.selected < 0 || !s.active) {
      next(following);
      return true;
    }
    if (!step_done_) {
      s.active = false;
      backend().closeStream(daddr_, s.alt().ep.bEndpointAddress);
      if (s.alt().has_fb)
        backend().closeStream(daddr_, s.alt().fb_ep.bEndpointAddress);
      return issueSetInterface(s.alt().itf, 0);
    }
    next(following);
    return true;
  }

  bool clockRangeHeader(Stream& s, std::vector<RateRange>& ranges,
                        Step on_success, Step skip) {
    if (uac_version_ != 2 || !s.present() || s.clock_id == 0 ||
        reconfiguring_) {
      next(skip);
      return true;
    }
    if (!step_done_) return requestClockRange(s, 2);
    if (!ctrl_ok_ || ctrl_len_ < 2) {
      ranges.clear();
      next(skip);
      return true;
    }
    next(on_success);
    return true;
  }

  bool clockRange(Stream& s, std::vector<RateRange>& ranges, Step following) {
    if (!step_done_) {
      uint16_t count = descU16(ctrl_buf_);
      uint32_t len = 2 + 12u * count;
      if (len > kCtrlBufSize) len = kCtrlBufSize;
      return requestClockRange(s, (uint16_t)len);
    }
    if (ctrl_ok_) parseClockRange(ranges);
    for (auto& r : ranges) {
      LOGI("Clock %d: %d - %d Hz (res %d)", s.clock_id, (int)r.min,
           (int)r.max, (int)r.res);
    }
    next(following);
    return true;
  }

  bool setClock(Stream& s, Step following) {
    if (uac_version_ != 2 || s.selected < 0 || s.clock_id == 0) {
      next(following);
      return true;
    }
    if (!step_done_) return requestSetClock(s);
    if (!ctrl_ok_) LOGW("Setting the sample rate to %d failed", (int)s.sample_rate);
    next(following);
    return true;
  }

  bool setInterface(Stream& s, Step on_success, Step skip) {
    if (s.selected < 0) {
      next(skip);
      return true;
    }
    if (!step_done_) return issueSetInterface(s.alt().itf, s.alt().alt);
    if (!ctrl_ok_) {
      LOGE("SET_INTERFACE %d/%d failed", s.alt().itf, s.alt().alt);
      s.selected = -1;
      next(skip);
      return true;
    }
    next(on_success);
    return true;
  }

  void openStream(Stream& s, bool is_rx) {
    AltSetting& a = s.alt();
    s.packets_per_sec = packetsPerSecond(a);
    s.nominal = (uint32_t)(((uint64_t)s.sample_rate << 16) / s.packets_per_sec);
    s.acc = 0;
    s.acc_fb = 0;
    s.feedback = 0;
    s.primed = false;
    s.active = true;
    bool ok = backend().openStream(
        daddr_, a.ep,
        is_rx ? UsbHostStreamRole::RxData : UsbHostStreamRole::TxData);
    if (ok && !is_rx && a.has_fb) {
      backend().openStream(daddr_, a.fb_ep, UsbHostStreamRole::Feedback);
    }
    if (!ok) {
      LOGE("Opening the endpoint 0x%x failed", a.ep.bEndpointAddress);
      s.active = false;
    }
  }

  bool setEpRate(Stream& s, Step following) {
    // UAC1: the sampling frequency control is on the endpoint
    if (uac_version_ != 1 || s.selected < 0 || s.alt().rate_count < 2) {
      next(following);
      return true;
    }
    if (!step_done_) return requestSetEpRate(s);
    if (!ctrl_ok_) LOGW("Setting the sample rate to %d failed", (int)s.sample_rate);
    next(following);
    return true;
  }

  bool volumeRange(Stream& s, bool max, Step on_success, Step skip) {
    if (s.selected < 0 || s.fu_id == 0 || s.fu_volume_mask == 0 ||
        reconfiguring_) {
      next(skip);
      return true;
    }
    if (uac_version_ == 2 && max) {
      next(on_success);
      return true;
    }
    if (!step_done_) return requestVolumeRange(s, max);
    if (!ctrl_ok_) {
      next(skip);
      return true;
    }
    if (uac_version_ == 2) {
      if (ctrl_len_ >= 6) {
        s.vol_min = (int16_t)descU16(ctrl_buf_ + 2);
        s.vol_max = (int16_t)descU16(ctrl_buf_ + 4);
      }
    } else if (ctrl_len_ >= 2) {
      int16_t v = (int16_t)descU16(ctrl_buf_);
      if (max)
        s.vol_max = v;
      else
        s.vol_min = v;
    }
    if (s.vol_min == (int16_t)0x8000) s.vol_min++;  // 0x8000 = silence
    if (s.vol_max <= s.vol_min) s.vol_max = s.vol_min + 1;
    next(on_success);
    return true;
  }

  void finishMount() {
    mounted_ = rx_.active || tx_.active;
    reconfiguring_ = false;
    if (!mounted_) {
      LOGE("USB audio device could not be started");
      return;
    }
    // report the negotiated format
    AudioInfo negotiated = rx_.active ? rx_info_ : tx_info_;
    if (!(negotiated == info)) {
      info = negotiated;
      config_.copyFrom(negotiated);
      if (volume_.size() != (size_t)negotiated.channels + 1) setupVolumeState();
      notifyAudioChange(negotiated);
    }
    if (sample_rate_cb_) sample_rate_cb_(negotiated.sample_rate);
    // apply the current volume and mute state to the device
    volume_pending_.assign(volume_.size(), 1);
    mute_pending_.assign(mute_.size(), 1);
    LOGI("USB audio streaming started (rx=%d, tx=%d)", rx_.active, tx_.active);
    notifyStreamingState();
  }

  /// Sends the next queued volume or mute change
  void startRuntimeRequest() {
    for (uint8_t ch = 0; ch < mute_pending_.size(); ch++) {
      if (mute_pending_[ch]) {
        mute_pending_[ch] = 0;
        if (sendFeature(false, ch)) return;
      }
    }
    for (uint8_t ch = 0; ch < volume_pending_.size(); ch++) {
      if (volume_pending_[ch]) {
        volume_pending_[ch] = 0;
        if (sendFeature(true, ch)) return;
      }
    }
  }

  /// Sends the volume or mute of a channel to the next stream which
  /// supports it. Returns true if a request was issued.
  bool sendFeature(bool is_volume, uint8_t ch) {
    // the first call starts with rx; finishRuntimeRequest continues with tx
    pending_is_volume_ = is_volume;
    pending_channel_ = ch;
    pending_stream_ = &rx_;
    return sendFeatureFor(rx_) || sendFeatureNextStream();
  }

  bool sendFeatureNextStream() {
    if (pending_stream_ == &rx_) {
      pending_stream_ = &tx_;
      if (sendFeatureFor(tx_)) return true;
    }
    pending_stream_ = nullptr;
    return false;
  }

  bool sendFeatureFor(Stream& s) {
    if (!s.active || s.fu_id == 0) return false;
    uint8_t ch = pending_channel_;
    if (ch > kMaxChannels) return false;
    uint16_t mask = pending_is_volume_ ? s.fu_volume_mask : s.fu_mute_mask;
    if ((mask & (1u << ch)) == 0) {
      // no master control: apply the master to the individual channels
      if (ch == 0) {
        for (uint8_t c = 1; c < volume_.size() && c <= kMaxChannels; c++) {
          if (mask & (1u << c)) {
            if (pending_is_volume_)
              volume_pending_[c] = 1;
            else
              mute_pending_[c] = 1;
          }
        }
      }
      return false;
    }
    if (pending_is_volume_) {
      float vol = volume(ch);
      if (ch != 0 && (s.fu_volume_mask & 0x01) == 0) vol *= volume(0);
      return requestFeature(s, AUDIO_FU_CTRL_VOLUME, ch, toDeviceVolume(s, vol));
    }
    bool m = isMute(ch);
    if (ch != 0 && (s.fu_mute_mask & 0x01) == 0) m = m || isMute(0);
    return requestFeature(s, AUDIO_FU_CTRL_MUTE, ch, m ? 1 : 0);
  }

  void finishRuntimeRequest() {
    if (!ctrl_ok_) LOGW("USB audio feature unit request failed");
    if (pending_stream_ != nullptr) sendFeatureNextStream();
  }

  /// Explicit feedback from the speaker: samples per packet
  void onFeedback(const uint8_t* data, uint16_t len) {
    uint32_t value = 0;
    if (len == 3) {
      value = descU24(data) << 2;  // 10.14 -> 16.16
    } else if (len >= 4) {
      value = descU32(data);  // 16.16
    } else {
      return;
    }
    // accept only plausible values (+/- 1/8 of nominal); some full speed
    // devices send the 10.14 format in 4 bytes
    uint32_t nom = tx_.nominal;
    if (nom == 0) return;
    uint32_t delta = nom >> 3;
    auto plausible = [&](uint32_t v) { return v > nom - delta && v < nom + delta; };
    if (plausible(value)) {
      tx_.feedback = value;
    } else if (len >= 4 && plausible(value << 2)) {
      tx_.feedback = value << 2;
    }
  }

  void resetDevice() {
    attached_ = false;
    configured_ = false;
    mounted_ = false;
    daddr_ = 0;
    vid_ = pid_ = 0;
    uac_version_ = 0;
    step_ = Step::Idle;
    ctrl_busy_ = false;
    reconfiguring_ = false;
    reconfigure_pending_ = false;
    restart_pending_ = false;
    pending_stream_ = nullptr;
    rx_.reset();
    tx_.reset();
    entities_.clear();
    clock_ranges_rx_.clear();
    clock_ranges_tx_.clear();
    volume_pending_.clear();
    mute_pending_.clear();
  }
};

}  // namespace audio_tools
