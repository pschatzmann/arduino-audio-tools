#pragma once

#include "AudioToolsConfig.h"

// LE Audio (BLEAudio) is provided by the ESP32 Arduino core (e.g. ESP32-S31).
// The class is only compiled when the LC3 codec is available (checked below).
#if defined(ESP32)
#define AUDIOTOOLS_LE_AUDIO_AVAILABLE 1
#include <BLE.h>
#include <BLEAudio.h>

#include "AudioTools/Concurrency/LockFree/RingBufferSPSC.h"
#include "AudioTools/CoreAudio/AudioStreams.h"
#include "AudioTools/CoreAudio/AudioBasic/StrView.h"

#if BLE_AUDIO_LC3_SUPPORTED

namespace audio_tools {

/**
 * @brief Configuration for AudioBLE
 * @ingroup communications
 */
class AudioBLEConfig {
 public:
  /// RX_MODE: we receive audio (sink, readBytes()); TX_MODE: we send audio (source, write())
  RxTxMode mode = RX_MODE;
  /// Name which is advertised to the peer
  const char* name = "LE Audio";
  /// Size of the PCM buffer in bytes (rounded up to a power of 2)
  int buffer_size = 16 * 1024;
};

/**
 * @brief Stream support for LE Audio (unicast) using the BLEAudio library of
 * the ESP32 Arduino core. The LC3 encoding/decoding is done by the core, the
 * data is exchanged as 16 bit interleaved PCM.
 *
 * - begin(RX_MODE, name): we are a sink: the peer streams audio to us and
 *   the decoded PCM is provided by readBytes() / available().
 * - begin(TX_MODE, name): we are a source: the PCM you write() is encoded
 *   and streamed to the peer.
 *
 * Only one instance per direction is supported. The PCM format is the one
 * negotiated by the peer (see audioInfo()); for TX the data must match it.
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

  /// Starts the processing in the indicated mode
  bool begin(RxTxMode mode, const char* name) {
    AudioBLEConfig cfg;
    cfg.mode = mode;
    cfg.name = name;
    return begin(cfg);
  }

  /// Starts the processing
  bool begin(AudioBLEConfig cfg) {
    TRACEI();
    config = cfg;
    rx_buffer.resize(cfg.buffer_size);

    BTStatus st = BLE.begin(cfg.name);
    if (!st) {
      LOGE("BLE.begin failed: %s", st.toString());
      return false;
    }
    audio = BLE.getAudioController();
    if (!audio) {
      LOGE("getAudioController failed");
      return false;
    }
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
      unicast_server.stream(::BLEAudioStream::Direction::Rx).onConfigured([this](::BLEAudioStream& s) {
        (void)s;
        start_rx_pending = true;
      });
      unicast_server.stream(::BLEAudioStream::Direction::Rx).onStopped([this](::BLEAudioStream&, uint8_t) {
        stop_pending = true;
      });
    } else {
      // one source ASE, no sink ASE
      st = unicast_server.configureHeadset();
      if (st) st = unicast_server.setSinkStreams(0);
      if (!st) {
        LOGE("source setup failed: %s", st.toString());
        return false;
      }
      unicast_server.stream(::BLEAudioStream::Direction::Tx).onConfigured([this](::BLEAudioStream& s) {
        negotiated_info.sample_rate = s.codecConfig().samplingRateHz;
        negotiated_info.channels = s.codecConfig().channels();
        negotiated_info.bits_per_sample = 16;
        start_tx_pending = true;
      });
      unicast_server.stream(::BLEAudioStream::Direction::Tx).onStopped([this](::BLEAudioStream&, uint8_t) {
        stop_pending = true;
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
    is_started = true;
    return true;
  }

  /// Stops the processing and clears the buffer
  void end() override {
    TRACEI();
    player.stopPlayback();
    player.end();
    recorder.stopCapture();
    recorder.end();
    rx_buffer.reset();
    is_started = false;
  }

  /// true while the audio is running (streaming with a peer)
  operator bool() override {
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

  /// The PCM format: as negotiated with the peer (valid once it is connected)
  AudioInfo audioInfo() override {
    if (config.mode == RX_MODE) return player.isRunning() ? player_info() : AudioInfo();
    return negotiated_info;
  }

  /// Provides access to the LE Audio player (RX) for statistics
  ::BLEAudioPlayer& getPlayer() { return player; }
  /// Provides access to the LE Audio recorder (TX) for statistics
  ::BLEAudioRecorder& getRecorder() { return recorder; }

 protected:
  AudioBLEConfig config;
  bool is_started = false;
  ::BLEAudio audio;
  ::BLEAudioCapAcceptor cap_acceptor;
  ::BLEAudioUnicastServer unicast_server;
  ::BLEAudioPlayer player;      // RX
  ::BLEAudioRecorder recorder;  // TX
  // single producer / single consumer: lock-free between the loop and the codec task
  RingBufferSPSC<uint8_t> rx_buffer;
  AudioInfo negotiated_info;
  AudioInfo pending_info;
  AudioInfo applied_info;
  volatile bool start_rx_pending = false;
  volatile bool start_tx_pending = false;
  volatile bool stop_pending = false;

  AudioInfo player_info() {
    AudioInfo info(player.sampleRate(), player.channels(), 16);
    return info;
  }

  /// Runs on the codec task (RX): store the decoded PCM
  void onPCM(const int16_t* pcm, size_t samples) {
    if (pcm == nullptr || samples == 0) return;
    rx_buffer.writeArray((const uint8_t*)pcm, samples * sizeof(int16_t));
  }

  /// Runs on the codec task (TX): provide PCM to encode
  size_t pullPCM(int16_t* pcm, size_t samples) {
    int bytes = rx_buffer.readArray((uint8_t*)pcm, samples * sizeof(int16_t));
    return bytes / sizeof(int16_t);
  }

  /// Runs on the loop: apply the requests of the BLE / codec tasks
  void poll() {
    if (start_rx_pending) {
      start_rx_pending = false;
      player.stopPlayback();
      if (player.attach(unicast_server.stream(::BLEAudioStream::Direction::Rx))) {
        player.startPlayback();
      }
    }
    if (start_tx_pending) {
      start_tx_pending = false;
      recorder.stopCapture();
      if (recorder.attach(unicast_server.stream(::BLEAudioStream::Direction::Tx))) {
        recorder.startCapture();
      }
    }
    if (stop_pending) {
      stop_pending = false;
      player.stopPlayback();
      recorder.stopCapture();
    }
  }
};

}  // namespace audio_tools

#endif  // BLE_AUDIO_LC3_SUPPORTED
#endif  // ESP32
