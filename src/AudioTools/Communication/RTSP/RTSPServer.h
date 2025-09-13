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

#include "AudioStreamer.h"
#include "RTSPSession.h"
#ifdef ESP32
#include <esp_wifi.h>
#endif

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
 * - Coordinates with AudioStreamer for RTP audio delivery
 * - Runs asynchronously using FreeRTOS tasks on ESP32
 *
 * @section usage Usage Example
 * @code
 * // Create audio source and streamer
 * MyAudioSource audioSource;
 * AudioStreamer streamer(&audioSource);
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
 * @note Requires ESP32 platform for task management and WiFi
 * @ingroup rtsp
 * @author Thomas Pfitzinger
 * @version 0.1.1
 */
class RTSPServer {
 public:
  /**
   * @brief Construct RTSP server
   *
   * Creates a new RTSP server instance configured to work with the specified
   * AudioStreamer. The server will listen for client connections and coordinate
   * streaming sessions.
   *
   * @param streamer Pointer to AudioStreamer that provides the audio data
   * source. Must remain valid for the server's lifetime.
   * @param port TCP port number for RTSP connections (default 8554 - standard
   * RTSP port)
   * @param core ESP32 core number to run server tasks on (0 or 1, default 1)
   *
   * @note The AudioStreamer must be properly configured with an audio source
   * @note Port 8554 is the IANA-assigned port for RTSP
   * @see begin(), runAsync()
   */
  RTSPServer(AudioStreamer& streamer, int port = 8554, int core = 1) {
    this->streamer = &streamer;
    this->port = port;
    this->core = core;
  }

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
  int begin(const char* ssid, const char* password) {
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
    return runAsync();
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
   * @note Server runs on the configured ESP32 core
   * @note Use this method when WiFi is already established
   * @see begin() for WiFi setup + server start
   */
  int runAsync() {
    int error;

    log_i("Running RTSP server on port %d", port);

    streamer->initAudioSource();

    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_addr.s_addr = INADDR_ANY;
    ServerAddr.sin_port = htons(port);  // listen on RTSP port 8554 as default
    int s = socket(AF_INET, SOCK_STREAM, 0);
    log_d("Master socket fd: %i", s);
    MasterSocket = new WiFiClient(s);
    if (MasterSocket == NULL) {
      log_e("MasterSocket object couldnt be created");
      return -1;
    }

    log_d("Master Socket created; fd: %i", MasterSocket->fd());

    int enable = 1;
    error = setsockopt(MasterSocket->fd(), SOL_SOCKET, SO_REUSEADDR, &enable,
                       sizeof(int));
    if (error < 0) {
      log_e("setsockopt(SO_REUSEADDR) failed");
      return error;
    }

    log_v("Socket options set");

    // bind our master socket to the RTSP port and listen for a client
    // connection
    error =
        bind(MasterSocket->fd(), (sockaddr*)&ServerAddr, sizeof(ServerAddr));
    if (error != 0) {
      log_e("error can't bind port errno=%d", errno);
      return error;
    }
    log_v("Socket bound. Starting to listen");
    error = listen(MasterSocket->fd(), 5);
    if (error != 0) {
      log_e("Error while listening");
      return error;
    }

    if (xTaskCreatePinnedToCore(RTSPServer::serverThread, "RTSPServerThread",
                                10000, (void*)this, 5, &serverTaskHandle,
                                core) != pdPASS) {
      log_e("Couldn't create server thread");
      return -1;
    }

    return 0;
  }

  TaskHandle_t getTaskHandle() { return serverTaskHandle; };

 protected:
  TaskHandle_t serverTaskHandle;
  TaskHandle_t sessionTaskHandle;
  SOCKET MasterSocket;  // our masterSocket(socket that listens for RTSP client
                        // connections)
  SOCKET ClientSocket;  // RTSP socket to handle an client
  sockaddr_in ServerAddr;  // server address parameters
  sockaddr_in ClientAddr;  // address parameters of a new RTSP client
  int port;                // port that the RTSP Server listens on
  int core;                // the ESP32 core number the RTSP runs on (0 or 1)

  int numClients = 0;  // number of connected clients

  AudioStreamer* streamer =
      nullptr;  // AudioStreamer object that acts as a source for data streams

  /**
   * @brief Main server thread - listens for RTSP client connections
   *
   * This static method runs in a dedicated FreeRTOS task and implements the
   * main server loop. It accepts incoming TCP connections from RTSP clients and
   * creates session threads to handle each client's RTSP protocol
   * communication.
   *
   * @param server_obj Void pointer to RTSPServer instance (cast from task
   * parameter)
   *
   * @note Currently supports one client at a time (single-threaded sessions)
   * @note Runs indefinitely until task is deleted
   * @note Creates sessionThread tasks for each accepted client
   */
  static void serverThread(void* server_obj) {
    socklen_t ClientAddrLen = sizeof(ClientAddr);
    RTSPServer* server = (RTSPServer*)server_obj;
    TickType_t prevWakeTime = xTaskGetTickCount();

    log_i("Server thread listening...");

    while (true) {  // loop forever to accept client connections
      // only allow one client at a time

      if (server->numClients == 0) {
        server->ClientSocket = new WiFiClient(
            accept(server->MasterSocket->fd(),
                   (struct sockaddr*)&server->ClientAddr, &ClientAddrLen));
        log_i("Client connected. Client address: %s",
              inet_ntoa(server->ClientAddr.sin_addr));
        if (xTaskCreatePinnedToCore(RTSPServer::sessionThread,
                                    "RTSPSessionTask", 8000, (void*)server, 8,
                                    &server->sessionTaskHandle,
                                    server->core) != pdPASS) {
          log_e("Couldn't create sessionThread");
        } else {
          log_d("Created sessionThread");
          server->numClients++;
        }
      }

      vTaskDelayUntil(&prevWakeTime, 200 / portTICK_PERIOD_MS);
    }

    // should never be reached
    closesocket(server->MasterSocket);

    log_e("Error: %s is returning", pcTaskGetTaskName(NULL));
  }

  /**
   * @brief Client session thread - handles RTSP protocol for individual clients
   *
   * This static method runs in a dedicated FreeRTOS task for each connected
   * client. It creates an RTSPSession object and processes RTSP requests
   * (DESCRIBE, SETUP, PLAY, TEARDOWN) until the client disconnects or an error
   * occurs.
   *
   * @param server_obj Void pointer to RTSPServer instance (cast from task
   * parameter)
   *
   * @note Each client gets its own session thread
   * @note Thread automatically terminates when client disconnects
   * @note Manages RTSPSession lifecycle and socket cleanup
   */
  static void sessionThread(void* server_obj) {
    RTSPServer* server = (RTSPServer*)server_obj;
    AudioStreamer* streamer = server->streamer;
    SOCKET s = server->ClientSocket;
    TickType_t prevWakeTime = xTaskGetTickCount();

    // stop this task - wait for a client to connect
    // vTaskSuspend(NULL);
    // TODO check if everything is ok to go
    log_v("RTSP Task running");

     // our threads RTSP session and state
    RtspSession* rtsp = new RtspSession(*s, *streamer); 

    log_i("Session ready");

    while (rtsp->m_sessionOpen) {
      uint32_t timeout = 30;
      if (!rtsp->handleRequests(timeout)) {
        log_v("Request handling timed out");

      } else {
        log_v("Request handling successful");
      }

      vTaskDelayUntil(&prevWakeTime, timeout / portTICK_PERIOD_MS);
    }

    // should never be reached
    log_i("sessionThread stopped, deleting task");
    delete rtsp;
    server->numClients--;

    vTaskDelete(NULL);
  }
};
