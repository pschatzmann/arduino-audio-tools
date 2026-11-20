#pragma once

#include "AudioToolsConfig.h"

// LE Audio (BLEAudio) is provided by the ESP32 Arduino core (e.g. ESP32-S31).
// The class is only compiled when the LC3 codec is available (checked below).
#if defined(ESP32)
#define AUDIOTOOLS_LE_AUDIO_AVAILABLE 1
#include <BLE.h>
#include <BLEAudio.h>

#include <atomic>

#include "AudioTools/Concurrency/LockFree/RingBufferSPSC.h"
#include "AudioTools/CoreAudio/AudioStreams.h"
#include "AudioTools/CoreAudio/AudioBasic/StrView.h"

#if BLE_AUDIO_LC3_SUPPORTED

namespace audio_tools {

/**
 * @brief Configuration for AudioBLE. It is an AudioInfo: the format is used
 * until the peer defines the real one. For TX it is also the format that is
 * offered to the peer (16, 24 or 48 kHz, mono or stereo).
 * @ingroup communications
 */
class AudioBLEConfig : public AudioInfo {
 public:
  AudioBLEConfig() {
    sample_rate = 48000;
    channels = 2;
    bits_per_sample = 16;
  }
  /// RX_MODE: we receive audio (sink, readBytes()); TX_MODE: we send audio (source, write())
  RxTxMode mode = RX_MODE;
  /// Name which is advertised to the peer
  const char* name = "LE Audio";
  /// Size of the PCM buffer in bytes (rounded up to a power of 2). At 48 kHz
  /// stereo 16 bit, 32 KB hold about 170 ms of audio.
  int buffer_size = 32 * 1024;
};

/**
 * @brief Stream support for LE Audio (unicast) using the BLEAudio library of
 * the ESP32 Arduino core. The LC3 encoding/decoding is done by the core, the
 * data is exchanged as 16 bit interleaved PCM.
 *
 * - begin(RX_MODE, name): we are a sink: the peer streams audio to us and
 *   the decoded PCM is provided by readBytes() / available().
 * - begin(TX_MODE, name): we are a source: the PCM you write() is encoded
 *   and streamed to the peer. Only the LC3 preset of the configured format is
 *   offered, so the peer must use that sample rate and channel count.
 *
 * The format is announced to the downstream listeners (notifyAudioChange())
 * when it is configured, and again when the peer defines it.
 *
 * The connection state changes are handled in poll(). It is called by
 * readBytes(), available(), write() and availableForWrite(), so call one of
 * them regularly (StreamCopy does) or call poll() yourself in loop().
 *
 * Only one instance is supported. begin() can be called again after end().
 *
 * Note: the core also has a global class named BLEAudioStream. Inside the
 * audio_tools namespace the core class is referred to as ::BLEAudioStream.
 *
 * @ingroup communications
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioBLE : public AudioStream {
 public:
  AudioBLE() = default;
  // the callbacks capture this: the object must not be copied
  AudioBLE(const AudioBLE&) = delete;
  AudioBLE& operator=(const AudioBLE&) = delete;
  // the BLE callbacks refer to this object: release them while it still exists
  ~AudioBLE() override { end(); }

  /// Provides the default configuration for the indicated mode
  AudioBLEConfig defaultConfig(RxTxMode mode = RX_MODE) {
    AudioBLEConfig cfg;
    cfg.mode = mode;
    return cfg;
  }

  /// Starts the processing in the indicated mode
  bool begin(RxTxMode mode, const char* name) {
    AudioBLEConfig cfg = defaultConfig(mode);
    cfg.name = name;
    return begin(cfg);
  }

  /// Starts the processing. Returns false if already started or on error
  /// (in that case everything that was set up is released again).
  bool begin(AudioBLEConfig cfg) {
    TRACEI();
    if (is_started) {
      LOGW("AudioBLE: already started");
      return false;
    }
    if (!rx_buffer.resize(cfg.buffer_size)) {
      LOGE("AudioBLE: allocation of %d bytes failed", cfg.buffer_size);
      return false;
    }
    config = cfg;
    rx_dropped_bytes = 0;
    if (!setup(cfg)) {
      end();  // release whatever was set up
      return false;
    }
    // initial format, until the peer defines it: only announced once started
    updateInfo(cfg);
    is_started = true;
    return true;
  }

  /// Stops the processing and releases the BLE stack: advertising stops and
  /// connected peers are disconnected. A new begin() starts it again.
  void end() override {
    TRACEI();
    // stop the data path first: the codec task must not use the buffer anymore
    if (player) {
      player.stopPlayback();
      player.end();
    }
    if (recorder) {
      recorder.stopCapture();
      recorder.end();
    }
    // the callbacks capture this: remove them before the roles are released
    if (unicast_server) {
      unicast_server.stream(::BLEAudioStream::Direction::Rx).resetCallbacks();
      unicast_server.stream(::BLEAudioStream::Direction::Tx).resetCallbacks();
    }
    if (audio) audio.resetCallbacks();
    if (ble_started) {
      BLE.end();  // stops advertising, disconnects and deinitializes the host
      ble_started = false;
    }
    unicast_server = ::BLEAudioUnicastServer();
    cap_acceptor = ::BLEAudioCapAcceptor();
    audio = ::BLEAudio();
    rx_buffer.reset();
    rx_event = NoEvent;
    tx_event = NoEvent;
    adv_restart_pending = false;
    is_started = false;
  }

  /// true while the audio is running (streaming with a peer)
  operator bool() override {
    if (!is_started) return false;
    if (config.mode == RX_MODE) return player.isRunning();
    return recorder.isRunning();
  }

  /// RX_MODE: provides the decoded PCM (16 bit interleaved)
  size_t readBytes(uint8_t* data, size_t len) override {
    poll();
    if (config.mode != RX_MODE) return 0;
    return rx_buffer.readArray(data, len);
  }

  /// RX_MODE: number of PCM bytes available to read
  int available() override {
    poll();
    if (config.mode != RX_MODE) return 0;
    return rx_buffer.available();
  }

  /// TX_MODE: writes PCM (16 bit interleaved) which is sent to the peer.
  /// Returns the number of bytes accepted (the buffer may be full).
  size_t write(const uint8_t* data, size_t len) override {
    poll();
    if (config.mode != TX_MODE) return 0;
    return rx_buffer.writeArray(data, len);
  }

  /// TX_MODE: number of bytes which can be written
  int availableForWrite() override {
    poll();
    if (config.mode != TX_MODE) return 0;
    return rx_buffer.availableForWrite();
  }

  /// RX_MODE: format of the stream is defined by the decoder. TX_MODE: the
  /// format is the one negotiated with the peer, so a different format from
  /// the upstream is ignored (with a warning).
  void setAudioInfo(AudioInfo newInfo) override {
    if (config.mode == TX_MODE) {
      // warn once per distinct rejected format, not on every call
      if (newInfo != AudioStream::audioInfo() && newInfo != rejected_info) {
        rejected_info = newInfo;
        LOGW("AudioBLE TX: ignoring format %d Hz %d ch, using %d Hz %d ch",
             (int)newInfo.sample_rate, (int)newInfo.channels,
             (int)AudioStream::audioInfo().sample_rate, (int)AudioStream::audioInfo().channels);
      }
      return;
    }
    AudioStream::setAudioInfo(newInfo);
  }

  /// RX_MODE: number of PCM bytes dropped because the buffer was full
  uint32_t droppedBytes() const { return rx_dropped_bytes.load(); }

  /// Handles the requests of the BLE and codec tasks (start / stop of the
  /// data path and format changes). Called from the loop side.
  void poll() {
    // advertise again after a peer has disconnected
    if (adv_restart_pending.exchange(false)) {
      // the core only restarts advertising for a GATT server, not for AudioBLE
      BTStatus st = BLE.getAdvertising().start();
      if (!st) LOGW("AudioBLE: advertising restart failed: %s", st.toString());
    }
    // RX: the format is defined by the decoder once the stream runs
    if (config.mode == RX_MODE && player.isRunning()) {
      AudioInfo format = player_info();
      if (format.sample_rate > 0 && format.channels > 0) {
        updateInfo(format);
      }
    }
    int rx = rx_event.exchange(NoEvent);
    if (rx == StartEvent) {
      player.stopPlayback();
      if (player.attach(unicast_server.stream(::BLEAudioStream::Direction::Rx))) {
        player.startPlayback();
      } else {
        LOGE("AudioBLE RX: attach failed");
      }
    } else if (rx == StopEvent) {
      player.stopPlayback();
    }
    int tx = tx_event.exchange(NoEvent);
    if (tx == StartEvent) {
      // TX: the format is the one negotiated with the peer
      AudioInfo format;
      format.sample_rate = tx_sample_rate.load();
      format.channels = tx_channels.load();
      format.bits_per_sample = 16;
      updateInfo(format);
      recorder.stopCapture();
      if (recorder.attach(unicast_server.stream(::BLEAudioStream::Direction::Tx))) {
        recorder.startCapture();
      } else {
        LOGE("AudioBLE TX: attach failed");
      }
    } else if (tx == StopEvent) {
      recorder.stopCapture();
    }
  }

  /// Provides access to the LE Audio player (RX) for statistics (e.g. sdusLost()).
  /// Do not call begin() or end() on it.
  ::BLEAudioPlayer& getPlayer() { return player; }
  /// Provides access to the LE Audio recorder (TX) for statistics (e.g. sdusSent()).
  /// Do not call begin() or end() on it.
  ::BLEAudioRecorder& getRecorder() { return recorder; }

 protected:
  AudioBLEConfig config;
  bool is_started = false;
  bool ble_started = false;
  ::BLEAudio audio;
  ::BLEAudioCapAcceptor cap_acceptor;
  ::BLEAudioUnicastServer unicast_server;
  ::BLEAudioPlayer player;      // RX
  ::BLEAudioRecorder recorder;  // TX
  // single producer / single consumer: lock-free between the loop and the codec task
  RingBufferSPSC<uint8_t> rx_buffer;
  // set on the BLE task, read in poll(): the flags are atomic, the values are
  // stored before the flag is set
  // latest request of the BLE task per direction: the last one wins
  enum EventType : int { NoEvent = 0, StartEvent = 1, StopEvent = 2 };
  std::atomic<int> rx_event{NoEvent};
  std::atomic<int> tx_event{NoEvent};
  std::atomic<bool> adv_restart_pending{false};
  std::atomic<uint32_t> tx_sample_rate{0};
  std::atomic<uint8_t> tx_channels{0};
  // written on the codec task
  std::atomic<uint32_t> rx_dropped_bytes{0};
  // last upstream format rejected in TX (used to log it only once)
  AudioInfo rejected_info;

  /// Sets up the BLE stack and the roles for the configuration
  bool setup(AudioBLEConfig cfg) {
    BTStatus st = BLE.begin(cfg.name);
    if (!st) {
      LOGE("BLE.begin failed: %s", st.toString());
      return false;
    }
    ble_started = true;
    audio = BLE.getAudioController();
    if (!audio) {
      LOGE("getAudioController failed");
      return false;
    }
    // a peer that disconnects is followed by advertising again (in poll())
    audio.onDisconnected([this](uint16_t) { adv_restart_pending = true; });
    cap_acceptor = audio.createCapAcceptor();
    if (!cap_acceptor || !cap_acceptor.setSetSize(1) || !cap_acceptor.setRank(1)) {
      LOGE("CAP acceptor setup failed");
      return false;
    }
    unicast_server = audio.createUnicastServer();
    if (!unicast_server) {
      LOGE("createUnicastServer failed");
      return false;
    }

    if (cfg.mode == RX_MODE) {
      // one sink ASE, no source ASE
      st = unicast_server.configureMediaSink();
      if (st) st = unicast_server.setSourceStreams(0);
      if (!st) {
        LOGE("sink setup failed: %s", st.toString());
        return false;
      }
      unicast_server.stream(::BLEAudioStream::Direction::Rx).onConfigured([this](::BLEAudioStream&) {
        rx_event = StartEvent;
      });
      unicast_server.stream(::BLEAudioStream::Direction::Rx).onStopped([this](::BLEAudioStream&, uint8_t) {
        rx_event = StopEvent;
      });
    } else {
      // one source ASE, no sink ASE; only the LC3 preset of the configured format
      uint32_t presets = presetMask(cfg);
      if (presets == 0) {
        LOGE("unsupported TX format: %d Hz, %d ch", (int)cfg.sample_rate, (int)cfg.channels);
        return false;
      }
      st = unicast_server.configureHeadset();
      if (st) st = unicast_server.setSupportedPresets(presets);
      if (st) st = unicast_server.setSinkStreams(0);
      if (!st) {
        LOGE("source setup failed: %s", st.toString());
        return false;
      }
      unicast_server.stream(::BLEAudioStream::Direction::Tx).onConfigured([this](::BLEAudioStream& s) {
        tx_sample_rate = s.codecConfig().samplingRateHz;
        tx_channels = s.codecConfig().channels();
        tx_event = StartEvent;
      });
      unicast_server.stream(::BLEAudioStream::Direction::Tx).onStopped([this](::BLEAudioStream&, uint8_t) {
        tx_event = StopEvent;
      });
    }

    st = audio.begin();
    if (!st) {
      LOGE("audio.begin failed: %s", st.toString());
      return false;
    }

    if (cfg.mode == RX_MODE) {
      st = player.setPCMSink([this](const int16_t* pcm, size_t samples) { onPCM(pcm, samples); });
      if (st) st = player.begin();
    } else {
      st = recorder.setPCMSource([this](int16_t* pcm, size_t samples) { return pullPCM(pcm, samples); });
      if (st) st = recorder.begin();
    }
    if (!st) {
      LOGE("player/recorder setup failed: %s", st.toString());
      return false;
    }

    ::BLEAdvertising adv = BLE.getAdvertising();
    adv.setType(BLEAdvType::ConnectableScannable);
    adv.setName(cfg.name);
    adv.setAppearance(BLE_APPEARANCE_GENERIC_AUDIO_SINK);
    st = adv.start();
    if (!st) {
      LOGE("advertising failed: %s", st.toString());
      return false;
    }
    return true;
  }

  AudioInfo player_info() {
    AudioInfo info(player.sampleRate(), player.channels(), 16);
    return info;
  }

  /// Runs on the codec task (RX): store the decoded PCM
  void onPCM(const int16_t* pcm, size_t samples) {
    if (pcm == nullptr || samples == 0) return;
    size_t bytes = samples * sizeof(int16_t);
    int written = rx_buffer.writeArray((const uint8_t*)pcm, bytes);
    if ((size_t)written < bytes) {
      rx_dropped_bytes.fetch_add(bytes - (size_t)written);
    }
  }

  /// Runs on the codec task (TX): provide PCM to encode
  size_t pullPCM(int16_t* pcm, size_t samples) {
    int bytes = rx_buffer.readArray((uint8_t*)pcm, samples * sizeof(int16_t));
    return bytes / sizeof(int16_t);
  }

  /// LC3 preset mask for the format (16/24/48 kHz, mono or stereo); 0 if not
  /// supported. 44.1 kHz is not supported (it needs the framed presets).
  static uint32_t presetMask(const AudioInfo& info) {
    bool mono = info.channels == 1;
    if (!mono && info.channels != 2) return 0;
    switch (info.sample_rate) {
      case 16000:
        return mono ? BLEAudioPresetBit(::BLEAudioCodecPreset::LC3_16_1_1) : BLEAudioPresetBit(::BLEAudioCodecPreset::LC3_16_2_1);
      case 24000:
        return mono ? BLEAudioPresetBit(::BLEAudioCodecPreset::LC3_24_1_1) : BLEAudioPresetBit(::BLEAudioCodecPreset::LC3_24_2_1);
      case 48000:
        return mono ? BLEAudioPresetBit(::BLEAudioCodecPreset::LC3_48_1_1) : BLEAudioPresetBit(::BLEAudioCodecPreset::LC3_48_2_1);
      default:
        return 0;
    }
  }

  /// Sets the format and notifies the downstream listeners (via
  /// AudioStream::setAudioInfo()) when it differs from the current one.
  void updateInfo(AudioInfo newInfo) {
    if (newInfo != AudioStream::audioInfo()) {
      AudioStream::setAudioInfo(newInfo);
    }
  }
};

}  // namespace audio_tools

#endif  // BLE_AUDIO_LC3_SUPPORTED
#endif  // ESP32
