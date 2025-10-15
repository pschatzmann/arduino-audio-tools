/*
 * Author: Phil Schatzmann
 *
 * Based on Micro-RTSP library:
 * https://github.com/geeksville/Micro-RTSP
 * https://github.com/Tomp0801/Micro-RTSP-Audio
 *
 */

#pragma once
#include "RTSPAudioStreamer.h"
#include "RTSPServerBase.h"
#ifdef ESP32
#include <WiFi.h>
#include <esp_wifi.h>

#include "AudioTools/Concurrency/RTOS.h"
#endif

namespace audio_tools {

/**
 * @brief RTSPServer - Multi-client Audio Streaming Server (task-based)
 *
 * This class extends RTSPServerBase to add task/thread management for handling RTSP sessions.
 * It schedules the main server loop and individual session loops as tasks, allowing concurrent
 * client handling on platforms that support threading (e.g., ESP32 with FreeRTOS).
 *
 * Usage:
 *   - Construct with a valid RTSPAudioStreamer and port number.
 *   - Call begin() to start the server and listen for clients.
 *   - Optionally set a session path callback for custom session handling.
 *
 * Inherits all protocol, session, and connection logic from RTSPServerBase.
 * Only task scheduling and concurrency logic is implemented here.
 * @tparam Platform Target hardware platform (e.g., Arduino, ESP32)
 * @ingroup rtsp
 */
template <typename Platform>
class RTSPServer : public RTSPServerBase<Platform> {
 public:
  using streamer_t = RTSPAudioStreamerBase<Platform>;

  RTSPServer(streamer_t& streamer, int port = 8554, int core = 1)
      : RTSPServerBase<Platform>(streamer, port), core(core) {}

  ~RTSPServer() { end(); }

#ifdef ESP32

  bool begin(const char* ssid, const char* password) {
    return RTSPServerBase<Platform>::begin(ssid, password);
  }
#endif

  bool begin() {
    if (!RTSPServerBase<Platform>::begin()) return false;
    if (!serverTask.begin([this]() { serverThreadLoop(); })) {
      LOGE("Couldn't start server thread");
      return false;
    }
    return true;
  }

  void end() {
    sessionTask.end();
    serverTask.end();
    RTSPServerBase<Platform>::end();
  }

 protected:
  int core = 0;
  audio_tools::Task serverTask{"RTSPServerThread", 10000, 5, core};
  audio_tools::Task sessionTask{"RTSPSessionTask", 8000, 8, core};

  void serverThreadLoop() {
    unsigned long lastCheck = millis();
    LOGD("Server thread listening... (numClients: %d)", this->client_count);
    int prevClientCount = this->client_count;
    this->acceptClient();
    // If a new client was accepted, start session task
    if (this->client_count > prevClientCount) {
      LOGI("Client connected");
      if (!sessionTask.begin([this]() { sessionThreadLoop(); })) {
        LOGE("Couldn't start sessionThread");
        Platform::closeSocket(&this->client);
        this->client_count--;
      } else {
        LOGI("Number of clients: %d", this->client_count);
      }
    } else if (this->client_count > 0) {
      LOGD("Waiting for current session to end (numClients: %d)", this->client_count);
    }
    int time = 200 - (millis() - lastCheck);
    if (time < 0) time = 0;
    delay(time);
  }

  void sessionThreadLoop() {
    LOGD("RTSP Task running");
    while (this->rtspSession && this->rtspSession->isSessionOpen()) {
      this->handleSession();
      delay(30);
    }
    delay(500);
    LOGI("Session cleaned up: (numClients: %d)", this->client_count);
    sessionTask.end();
  }
};

}  // namespace audio_tools
