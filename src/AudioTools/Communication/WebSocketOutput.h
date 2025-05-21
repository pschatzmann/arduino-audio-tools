#pragma once
#include "AudioTools/CoreAudio/AudioOutput.h"
#include "WebSocketsClient.h"  // https://github.com/Links2004/arduinoWebSockets
#include "WebSocketsServer.h"  // https://github.com/Links2004/arduinoWebSockets

namespace audio_tools {

/**
 * @brief A simple wrapper class that lets you use the standard
 * Arduino Print class output commands to send audio data over a WebSocket
 * connection.
 * Uses https://github.com/Links2004/arduinoWebSockets
 * @ingroup communications
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class WebSocketOutput : public AudioOutput {
 public:
  WebSocketOutput() = default;
  /// @brief Constructor which defines an alternative WebSocket object. By
  /// default we use WebSocket
  WebSocketOutput(WebSocketsClient &ws) { setWebSocket(ws); }
  /// @brief Constructor which defines an alternative WebSocket object. By
  /// default we use WebSocket
  WebSocketOutput(WebSocketsServer &ws) { setWebSocket(ws); }

  /// @brief Defines an alternative WebSocket object. By default we use
  /// WebSocket
  void setWebSocket(WebSocketsClient &ws) { p_ws = &ws; };
  /// @brief Defines an alternative WebSocket object. By default we use
  /// WebSocket
  void setWebSocket(WebSocketsServer &ws) { p_ws_server = &ws; };

  /// Replys will be sent to the initial remote caller
  size_t write(const uint8_t *data, size_t len) override {
    bool rc = false;
    if (p_ws != nullptr) rc = p_ws->sendBIN(data, len);
    if (p_ws_server != nullptr) {
      if (clientNo >= 0) {
        rc = p_ws_server->sendBIN(clientNo, data, len);
      } else {
        // broadcast to all clients
        rc = p_ws_server->broadcastBIN(data, len);
      }
      rc = p_ws_server->broadcastBIN(data, len);
    }
    return rc ? len : 0;
  }

  /// For WebSocketServer we can define an individual recipient!
  void setTargetNo(int clientNo) { this->clientNo = clientNo; }

 protected:
  WebSocketsClient *p_ws = nullptr;
  WebSocketsServer *p_ws_server = nullptr;
  int clientNo = -1;
};

}  // namespace audio_tools
