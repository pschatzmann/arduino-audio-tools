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
#ifdef ESP32
#include <WiFi.h>
#include <esp_wifi.h>

#include "AudioTools/Concurrency/RTOS.h"
#endif

namespace audio_tools {

/**
 * @brief RTSP Server - Multi-client Audio Streaming Server
 *
 * The RTSPServer class implements a complete RTSP (Real Time Streaming
 * Protocol) server that manages client connections and coordinates audio
 * streaming sessions. This server:
 *
 * - Listens for RTSP client connections on a configurable port (default 8554)
 * - Handles RTSP protocol negotiation (DESCRIBE, SETUP, PLAY, PAUSE, TEARDOWN)
 * - Manages multiple concurrent client sessions
 * - Coordinates with RTSPAudioStreamer for RTP audio delivery
 * - Runs asynchronously using AudioTools Task system
 *
 * @section protocol RTSP Protocol Support
 * - DESCRIBE: Returns SDP session description with audio format
 * - SETUP: Establishes RTP transport parameters
 * - PLAY: Starts audio streaming to client
 * - PAUSE: Temporarily stops streaming without ending session
 * - TEARDOWN: Stops streaming and cleans up session
 * - OPTIONS: Returns supported RTSP methods
 *
 * @note Supports multiple platforms through AudioTools Task and Timer systems
 * @ingroup rtsp
 * @author Thomas Pfitzinger
 * @version 0.2.0
 */
template <typename Platform>
class RTSPServer {
 public:
  using streamer_t = RTSPAudioStreamer<Platform>;

  /**
   * @brief Construct RTSP server
   *
   * Creates a new RTSP server instance configured to work with the specified
   * RTSPAudioStreamer. The server will listen for client connections and
   * coordinate streaming sessions.
   *
   * @param streamer Pointer to RTSPAudioStreamer that provides the audio data
   * source. Must remain valid for the server's lifetime.
   * @param port TCP port number for RTSP connections (default 8554 - standard
   * RTSP port)
   * @param core Core number to run server tasks on (platform-specific, default
   * 1)
   *
   * @note The RTSPAudioStreamer must be properly configured with an audio
   * source
   * @note Port 8554 is the IANA-assigned port for RTSP
   * @see begin(), runAsync()
   */
  RTSPServer(streamer_t& streamer, int port = 8554, int core = 1) {
    this->streamer = &streamer;
    this->port = port;
    this->core = core;
    // Create tasks with specified core
    serverTask.create("RTSPServerThread", 10000, 5, core);
    sessionTask.create("RTSPSessionTask", 8000, 8, core);
  }

  /**
   * @brief Destructor - ensures proper cleanup of server resources
   */
  ~RTSPServer() { stop(); }

  /**
   * @brief Initialize WiFi and start RTSP server
   *
   * Convenience method that connects to a WiFi network and then starts the RTSP
   * server. This method blocks until WiFi connection is established, then
   * starts the server asynchronously.
   *
   * @param ssid WiFi network name to connect to
   * @param password WiFi network password
   * @return 0 on success, non-zero on failure
   *
   * @note Blocks until WiFi connection is established
   * @note Automatically calls runAsync() after WiFi setup
   * @see runAsync()
   */
  bool begin(const char* ssid, const char* password) {
#ifdef ESP32
    // Start Wifi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }

    esp_wifi_set_ps(WIFI_PS_NONE);
#endif

    Serial.println();
    Serial.print("connect to rtsp://");
    Serial.print(WiFi.localIP());
    Serial.print(":");
    Serial.println(port);
    return begin();
  }

  /** Start the RTSP server */
  bool begin() {
    bool ok = runAsync();
    if (!ok) {
      LOGE("Couldn't start RTSP server");
    }
    return ok;
  }

  /**
   * @brief Start RTSP server asynchronously
   *
   * Begins listening for RTSP client connections on the configured port.
   * The server runs in a separate FreeRTOS task, allowing the main program
   * to continue executing. Client sessions are handled in additional tasks.
   *
   * @return true on success, false on failure
   *
   * @note WiFi must be connected before calling this method
   * @note Server runs on the configured core (if platform supports it)
   * @note Use this method when WiFi is already established
   * @see begin() for WiFi setup + server start
   */
  bool runAsync() {
    LOGI("Running RTSP server on port %d", port);
    streamer->initAudioSource();
    if (server == nullptr) {
      server = Platform::createServer(port);
      LOGI("RTSP server started on port %d", port);
    }
    if (!serverTask.begin([this]() { serverThreadLoop(); })) {
      LOGE("Couldn't start server thread");
      return false;
    }
    return true;
  }

  audio_tools::Task& getTaskHandle() { return serverTask; };

 protected:
  int port;  // port that the RTSP Server listens on
  int core;  // the core number the RTSP runs on (platform-specific)
  audio_tools::Task serverTask{"RTSPServerThread", 15000, 5, core};
  audio_tools::Task sessionTask{"RTSPSessionTask", 15000, 8, core};
  typename Platform::TcpServerType* server = nullptr;  // platform server
  typename Platform::TcpClientType client;  // RTSP client connection (value)
  int numClients = 0;  // number of connected clients
  streamer_t* streamer = nullptr;  // RTSPAudioStreamer object that acts as a
                                   // source for data streams

  /**
   * @brief Main server thread loop - listens for RTSP client connections
   *
   * This member method implements the main server loop. It accepts incoming TCP
   * connections from RTSP clients and creates session threads to handle each
   * client's RTSP protocol communication.
   *
   * @note Currently supports one client at a time (single-threaded sessions)
   * @note Runs indefinitely in the task loop
   * @note Creates sessionThread tasks for each accepted client
   */
  void serverThreadLoop() {
    unsigned long lastCheck = millis();

    LOGD("Server thread listening... (numClients: %d)", numClients);

    // only allow one client at a time
    if (numClients == 0) {
      auto newClient = Platform::getAvailableClient(server);
      if (newClient && newClient.connected()) {
        client = newClient;  // copy/move assign
        LOGI("Client connected");
        if (!sessionTask.begin([this]() { sessionThreadLoop(); })) {
          LOGE("Couldn't start sessionThread");
          Platform::closeSocket(&client);
        } else {
          LOGD("Created sessionThread");
          numClients++;
        }
      }
    } else {
      LOGD("Waiting for current session to end (numClients: %d)", numClients);
    }
    int time = 200 - (millis() - lastCheck);
    if (time < 0) time = 0;
    delay(time);
  }

  /**
   * @brief Stop the RTSP server and cleanup resources
   */
  void stop() {
    LOGI("Stopping RTSP server");

    // Stop tasks
    sessionTask.end();
    serverTask.end();

    // Close sockets
    if (server) {
      server->stop();
      delete server;
      server = nullptr;
    }

    numClients = 0;
    LOGI("RTSP server stopped");
  }

  /**
   * @brief Client session thread loop - handles RTSP protocol for individual
   * clients
   *
   * This member method runs in a dedicated Task for each connected
   * client. It creates an RTSPSession object and processes RTSP requests
   * (DESCRIBE, SETUP, PLAY, TEARDOWN) until the client disconnects or an error
   * occurs.
   *
   * @note Each client gets its own session thread
   * @note Thread automatically terminates when client disconnects
   * @note Manages RTSPSession lifecycle and socket cleanup
   */
  void sessionThreadLoop() {
    typename Platform::TcpClientType* s = &client;
    unsigned long lastCheck = millis();

    // TODO check if everything is ok to go
    LOGD("RTSP Task running");

    // our threads RTSP session and state
    RtspSession<Platform>* rtsp = new RtspSession<Platform>(*s, *streamer);
    assert(rtsp != nullptr);

    LOGI("Session ready");

    while (rtsp->isSessionOpen()) {
      uint32_t timeout = 30;
      if (!rtsp->handleRequests(timeout)) {
        LOGD("Request handling timed out");
      } else {
        LOGD("Request handling successful");
      }

      int time = timeout - (millis() - lastCheck);
      if (time < 0) time = 0;
      delay(time);
    }

    LOGI("Session loop exited - session no longer open");

    // cleanup when session ends
    LOGI("sessionThread stopped, cleaning up");
    delete rtsp;

    // Clean up client socket
    if (client.connected()) {
      Platform::closeSocket(&client);
    }

    // Add delay to ensure complete cleanup before accepting new connections
    delay(500);  

    numClients--;
    LOGI("Session cleaned up: (numClients: %d)", numClients);

    // end the task - this should be the last thing we do
    sessionTask.end();
  }
};

}  // namespace audio_tools
