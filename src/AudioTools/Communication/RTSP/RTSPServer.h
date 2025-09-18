/*
 * Author: Phil Schatzmann
 *
 * Based on Micro-RTSP library:
 * https://github.com/geeksville/Micro-RTSP
 * https://github.com/Tomp0801/Micro-RTSP-Audio
 *
 */

#pragma once
#ifdef __linux__
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "AudioTools/Concurrency/Linux.h"

#else
#include "AudioTools/Concurrency/RTOS.h"
#endif
#include "RTSPSession.h"
#ifdef ESP32
#include <WiFi.h>
#include <esp_wifi.h>
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
    // Start Wifi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }

#ifdef ESP32
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
    int rc = runAsync();
    if (rc != 0) {
      LOGE("Couldn't start RTSP server: %d", rc);
    }
    return rc == 0;
  }

  /**
   * @brief Start RTSP server asynchronously
   *
   * Begins listening for RTSP client connections on the configured port.
   * The server runs in a separate FreeRTOS task, allowing the main program
   * to continue executing. Client sessions are handled in additional tasks.
   *
   * @return 0 on success, non-zero error code on failure
   *
   * @note WiFi must be connected before calling this method
   * @note Server runs on the configured core (if platform supports it)
   * @note Use this method when WiFi is already established
   * @see begin() for WiFi setup + server start
   */
  int runAsync() {
    int error;

    LOGI("Running RTSP server on port %d", port);

    streamer->initAudioSource();

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);  // listen on RTSP port 8554 as default
    int s = socket(AF_INET, SOCK_STREAM, 0);
    LOGD("Master socket fd: %i", s);
    masterSocket = new typename Platform::TcpClientType(s);
    if (masterSocket == NULL) {
      LOGE("MasterSocket object couldnt be created");
      return -1;
    }

    LOGD("Master Socket created; fd: %i", masterSocket->fd());

    int enable = 1;
    error = setsockopt(masterSocket->fd(), SOL_SOCKET, SO_REUSEADDR, &enable,
                       sizeof(int));
    if (error < 0) {
      LOGE("setsockopt(SO_REUSEADDR) failed");
      return error;
    }

    LOGD("Socket options set");

    // bind our master socket to the RTSP port and listen for a client
    // connection
    error =
        bind(masterSocket->fd(), (sockaddr*)&serverAddr, sizeof(serverAddr));
    if (error != 0) {
      LOGE("error can't bind port errno=%d", errno);
      return error;
    }
    LOGD("Socket bound. Starting to listen");
    error = listen(masterSocket->fd(), 5);
    if (error != 0) {
      LOGE("Error while listening");
      return error;
    }

    if (!serverTask.begin([this]() { serverThreadLoop(); })) {
      LOGE("Couldn't start server thread");
      return -1;
    }

    return 0;
  }

  audio_tools::Task& getTaskHandle() { return serverTask; };

 protected:
  int port;  // port that the RTSP Server listens on
  int core;  // the core number the RTSP runs on (platform-specific)
  audio_tools::Task serverTask{"RTSPServerThread", 15000, 5, core};
  audio_tools::Task sessionTask{"RTSPSessionTask", 15000, 8, core};
  typename Platform::TcpClientType*
      masterSocket;  // our masterSocket(socket that listens for RTSP client
                     // connections)
  typename Platform::TcpClientType*
      clientSocket;        // RTSP socket to handle an client
  sockaddr_in serverAddr;  // server address parameters
  sockaddr_in clientAddr;  // address parameters of a new RTSP client

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
    socklen_t ClientAddrLen = sizeof(clientAddr);
    unsigned long lastCheck = millis();

    LOGD("Server thread listening... (numClients: %d)", numClients);

    // only allow one client at a time
    if (numClients == 0) {
      int client_fd = accept(masterSocket->fd(), (struct sockaddr*)&clientAddr,
                             &ClientAddrLen);

      if (client_fd >= 0) {
        // Valid connection accepted
        clientSocket = new typename Platform::TcpClientType(client_fd);

        if (clientSocket && clientSocket->connected()) {
          LOGI("Client connected. Client address: %s",
               inet_ntoa(clientAddr.sin_addr));

          if (!sessionTask.begin([this]() { sessionThreadLoop(); })) {
            LOGE("Couldn't start sessionThread");
            delete clientSocket;  // Clean up on failure
            clientSocket = nullptr;
          } else {
            LOGD("Created sessionThread");
            numClients++;
          }
        } else {
          // Clean up failed connection
          LOGW("Failed to establish client connection");
          if (clientSocket) {
            delete clientSocket;
            clientSocket = nullptr;
          }
        }
      } else {
        // No pending connection, this is normal
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
          LOGE("Accept failed with error: %d", errno);
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
    if (masterSocket) {
      Platform::closeSocket(masterSocket);
      masterSocket = nullptr;
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
    typename Platform::TcpClientType* s = clientSocket;
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
    if (clientSocket) {
      delete clientSocket;
      clientSocket = nullptr;
    }

    // Add delay to ensure complete cleanup before accepting new connections
    delay(500);  // Give time for streamer cleanup to complete

    numClients--;
    LOGI(
        "Session cleanup completed, ready for new connections (numClients: %d)",
        numClients);

    // end the task - this should be the last thing we do
    sessionTask.end();
  }
};

}  // namespace audio_tools
