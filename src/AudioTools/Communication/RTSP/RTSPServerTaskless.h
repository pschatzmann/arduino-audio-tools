/*
 * Author: Phil Schatzmann
 *
 * Based on Micro-RTSP library:
 * https://github.com/geeksville/Micro-RTSP
 * https://github.com/Tomp0801/Micro-RTSP-Audio
 *
 */
#pragma once
#include "RTSPSession.h"
#include "RTSPAudioStreamer.h"
#include "RTSPServerBase.h"
#ifdef ESP32
#include <WiFi.h>
#include <esp_wifi.h>
#endif

namespace audio_tools {

/**
 * @brief RTSPServerTaskless - Manual Multi-client RTSP Audio Streaming Server
 *
 * This class implements an RTSP audio streaming server for platforms like Arduino and ESP32,
 * designed for environments where background tasks or threads are not available or desired.
 *
 * RTSPServerTaskless inherits all protocol and session management logic from RTSPServerBase,
 * but does not create any background tasks. Instead, it exposes a `doLoop()` method that must
 * be called frequently from the main Arduino loop (or equivalent) to handle client connections,
 * session management, and audio streaming.
 *
 * Key Features:
 * - Multi-client RTSP audio streaming over WiFi or Ethernet
 * - No background tasks: all server logic is executed synchronously in `doLoop()`
 * - Compatible with platforms where FreeRTOS or threading is unavailable
 * - Inherits all configuration and session management from RTSPServerBase
 * - Designed for maximum control and minimal resource usage
 *
 * @tparam Platform Target hardware platform (e.g., Arduino, ESP32)
 *
 * @ingroup rtsp
 */
template <typename Platform>
class RTSPServerTaskless : public RTSPServerBase<Platform> {
 public:
  RTSPServerTaskless(RTSPAudioStreamerBase<Platform>& streamer, int port = 8554)
      : RTSPServerBase<Platform>(streamer, port) {}

  ~RTSPServerTaskless() { this->end(); }

  void setOnSessionPath(bool (*cb)(const char* path, void* ref), void* ref = nullptr) {
    this->RTSPServerBase<Platform>::setOnSessionPath(cb, ref);
  }

#ifdef ESP32
  bool begin(const char* ssid, const char* password) {
    return this->RTSPServerBase<Platform>::begin(ssid, password);
  }
#endif

  bool begin() {
    return this->RTSPServerBase<Platform>::begin();
  }

  void end() {
    this->RTSPServerBase<Platform>::end();
  }

  int clientCount() { return this->RTSPServerBase<Platform>::clientCount(); }
  operator bool() { return this->RTSPServerBase<Platform>::operator bool(); }
  void setSessionTimeoutMs(unsigned long ms) { this->RTSPServerBase<Platform>::setSessionTimeoutMs(ms); }

  /**
   * @brief Main server loop - call this frequently from Arduino loop()
   */
  void doLoop() {
    this->RTSPServerBase<Platform>::acceptClient();
    this->RTSPServerBase<Platform>::handleSession();
  }
};

} // namespace audio_tools
