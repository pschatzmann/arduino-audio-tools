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
    typename Platform::TcpClientType* s = &this->client;
    unsigned long lastCheck = millis();
    LOGD("RTSP Task running");
    RtspSession<Platform>* rtsp = createSession(s);
    unsigned long lastRequestTime = millis();
    processSessionLoop(rtsp, lastCheck, lastRequestTime);
    cleanupSession(rtsp);
  }

  RtspSession<Platform>* createSession(typename Platform::TcpClientType* s) {
    RtspSession<Platform>* rtsp = new RtspSession<Platform>(*s, *this->streamer);
    if (this->onSessionPathCb) {
      rtsp->setOnSessionPath(this->onSessionPathCb, this->onSessionPathRef);
    }
    assert(rtsp != nullptr);
    LOGI("Session ready");
    return rtsp;
  }

  void processSessionLoop(RtspSession<Platform>* rtsp, unsigned long lastCheck, unsigned long lastRequestTime) {
    while (rtsp->isSessionOpen()) {
      uint32_t timeout = 30;
      bool gotRequest = rtsp->handleRequests(timeout);
      if (gotRequest) {
        lastRequestTime = millis();
        LOGD("Request handling successful");
      } else {
        if (rtsp->isStreaming()) {
          LOGI("Request handling timed out or no data yet");
        }
      }
      if (this->sessionTimeoutMs > 0 && rtsp->isStreaming()) {
        if ((millis() - lastRequestTime) > this->sessionTimeoutMs) {
          LOGW("Session timeout: no client request received for 20 seconds, closing session");
          break;
        }
      }
      int time = timeout - (millis() - lastCheck);
      if (time < 0) time = 0;
      delay(time);
    }
    LOGI("Session loop exited - session no longer open");
    LOGI("sessionThread stopped, cleaning up");
  }

  void cleanupSession(RtspSession<Platform>* rtsp) {
    delete rtsp;
    if (this->client.connected()) {
      Platform::closeSocket(&this->client);
    }
    delay(500);
    this->client_count--;
    LOGI("Session cleaned up: (numClients: %d)", this->client_count);
    sessionTask.end();
  }
};

}  // namespace audio_tools
