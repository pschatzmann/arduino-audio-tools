/*
 * Author: Thomas Pfitzinger
 * github: https://github.com/Tomp0801/Micro-RTSP-Audio
 *
 * Based on Micro-RTSP library for video streaming by Kevin Hester:
 *
 * https://github.com/geeksville/Micro-RTSP
 *
 * Copyright 2018 S. Kevin Hester-Chow, kevinh@geeksville.com (MIT License)
 */

#pragma once

#include "AudioTools/Concurrency/RTOS.h"
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
 * - Handles RTSP protocol negotiation (DESCRIBE, SETUP, PLAY, TEARDOWN)
 * - Manages multiple concurrent client sessions
 * - Coordinates with RTSPAudioStreamer for RTP audio delivery
 * - Runs asynchronously using AudioTools Task system
 *
 * @section usage Usage Example
 * @code
 * // Create audio source and streamer
 * MyAudioSource audioSource;
 * RTSPAudioStreamer streamer(&audioSource);
 *
 * // Create and start RTSP server
 * RTSPServer server(&streamer);
 *
 * // Option 1: Start with WiFi setup
 * server.begin("MySSID", "password");
 *
 * // Option 2: Start on existing network
 * server.runAsync();  // Default port 8554
 *
 * // Server runs in background, handling clients automatically
 * @endcode
 *
 * @section protocol RTSP Protocol Support
 * - DESCRIBE: Returns SDP session description with audio format
 * - SETUP: Establishes RTP transport parameters
 * - PLAY: Starts audio streaming to client
 * - TEARDOWN: Stops streaming and cleans up session
 * - OPTIONS: Returns supported RTSP methods
 *
 * @note Supports multiple platforms through AudioTools Task and Timer systems
 * @ingroup rtsp
 * @author Thomas Pfitzinger
 * @version 0.2.0
 */
template<typename Platform>
class RTSPServer {
 public:
  using streamer_t = RTSPAudioStreamer<Platform>;
  
  /**
   * @brief Construct RTSP server
   *
   * Creates a new RTSP server instance configured to work with the specified
   * RTSPAudioStreamer. The server will listen for client connections and coordinate
   * streaming sessions.
   *
   * @param streamer Pointer to RTSPAudioStreamer that provides the audio data
   * source. Must remain valid for the server's lifetime.
   * @param port TCP port number for RTSP connections (default 8554 - standard
   * RTSP port)
   * @param core Core number to run server tasks on (platform-specific, default 1)
   *
   * @note The RTSPAudioStreamer must be properly configured with an audio source
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
  bool begin(){
    int rc = runAsync();
    if (rc!=0){
      log_e("Couldn't start RTSP server: %d",rc);
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

    log_i("Running RTSP server on port %d", port);

    streamer->initAudioSource();

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);  // listen on RTSP port 8554 as default
    int s = socket(AF_INET, SOCK_STREAM, 0);
    log_d("Master socket fd: %i", s);
    masterSocket = new Platform::TcpClientType(s);
    if (masterSocket == NULL) {
      log_e("MasterSocket object couldnt be created");
      return -1;
    }

    log_d("Master Socket created; fd: %i", masterSocket->fd());

    int enable = 1;
    error = setsockopt(masterSocket->fd(), SOL_SOCKET, SO_REUSEADDR, &enable,
                       sizeof(int));
    if (error < 0) {
      log_e("setsockopt(SO_REUSEADDR) failed");
      return error;
    }

    log_v("Socket options set");

    // bind our master socket to the RTSP port and listen for a client
    // connection
    error =
        bind(masterSocket->fd(), (sockaddr*)&serverAddr, sizeof(serverAddr));
    if (error != 0) {
      log_e("error can't bind port errno=%d", errno);
      return error;
    }
    log_v("Socket bound. Starting to listen");
    error = listen(masterSocket->fd(), 5);
    if (error != 0) {
      log_e("Error while listening");
      return error;
    }

    if (!serverTask.begin([this]() { serverThreadLoop(); })) {
      log_e("Couldn't start server thread");
      return -1;
    }

    return 0;
  }

  audio_tools::Task& getTaskHandle() { return serverTask; };

 protected:
  int port;                // port that the RTSP Server listens on
  int core;                // the core number the RTSP runs on (platform-specific)
  audio_tools::Task serverTask{"RTSPServerThread", 15000, 5, core};
  audio_tools::Task sessionTask{"RTSPSessionTask", 15000, 8, core};
  Platform::TcpClientType* masterSocket;  // our masterSocket(socket that listens for RTSP client
                        // connections)
  Platform::TcpClientType* clientSocket;  // RTSP socket to handle an client
  sockaddr_in serverAddr;  // server address parameters
  sockaddr_in clientAddr;  // address parameters of a new RTSP client

  int numClients = 0;  // number of connected clients

  streamer_t* streamer =
      nullptr;  // RTSPAudioStreamer object that acts as a source for data streams

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

    log_i("Server thread listening...");

    // only allow one client at a time
    if (numClients == 0) {
      clientSocket = new Platform::TcpClientType(accept(
          masterSocket->fd(), (struct sockaddr*)&clientAddr, &ClientAddrLen));

      if (clientSocket && clientSocket->connected()) {
        log_i("Client connected. Client address: %s",
              inet_ntoa(clientAddr.sin_addr));

        if (!sessionTask.begin([this]() { sessionThreadLoop(); })) {
          log_e("Couldn't start sessionThread");
        } else {
          log_d("Created sessionThread");
          numClients++;
        }
      }
    }
    int time = 200 - (millis() - lastCheck);
    if (time < 0) time = 0;
    delay(time);
  }

  /**
   * @brief Stop the RTSP server and cleanup resources
   */
  void stop() {
    log_i("Stopping RTSP server");

    // Stop tasks
    sessionTask.end();
    serverTask.end();

    // Close sockets
    if (masterSocket) {
      Platform::closeSocket(masterSocket);
      masterSocket = nullptr;
    }

    numClients = 0;
    log_i("RTSP server stopped");
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
    log_v("RTSP Task running");

    // our threads RTSP session and state
    RtspSession<Platform>* rtsp = new RtspSession<Platform>(*s, *streamer);
    assert(rtsp != nullptr);

    log_i("Session ready");

    while (rtsp->isSessionOpen()) {
      uint32_t timeout = 30;
      if (!rtsp->handleRequests(timeout)) {
        log_v("Request handling timed out");
      } else {
        log_v("Request handling successful");
      }

      int time = timeout - (millis() - lastCheck);
      if (time < 0) time = 0;
      delay(time);
    }

    // cleanup when session ends
    log_i("sessionThread stopped, cleaning up");
    delete rtsp;
    numClients--;

    // end the task
    sessionTask.end();
  }
};

}  // namespace audio_tools
