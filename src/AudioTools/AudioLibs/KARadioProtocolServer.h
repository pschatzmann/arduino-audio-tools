#pragma once
#include "AudioTools/CoreAudio/BaseStream.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "HttpServer.h"
#include "KARadioProtocol.h"

namespace audio_tools {

/***
 * @brief KA-Radio Protocol: We can use the KA-Radio protocol over http to
 * control the audio player provided by the audiotools.
 * @author Phil Schatzmann
 */
class KARadioProtocolServer {
 public:
  /// Default constructor
  KARadioProtocolServer(AudioPlayer& player, int port = 80,
                        const char* ssid = nullptr, const char* pwd = nullptr) {
    setPlayer(player);
    setPort(port);
    setSSID(ssid);
    setPassword(pwd);
  }

  /// Empty constructor: call setPlayer to define the player
  KARadioProtocolServer() = default;

  /// Defines the player
  void setPlayer(AudioPlayer& player) { protocol.setPlayer(player); }

  void setPort(int port) { this->port = port; }
  void setSSID(const char* ssid) { this->ssid = ssid; }
  void setPassword(const char* password) { this->password = password; }
  void setSSID(const char* ssid, const char* password) {
    this->ssid = ssid;
    this->password = password;
  }

  bool begin() {
    context[0] = this;
    server.on("/", T_GET, parse, context.data(), context.size());

    // connect to WIFI
    if (ssid != nullptr && password != nullptr) {
      return server.begin(port, ssid, password);
    }
    return server.begin(port);
  }

  void loop() { server.copy(); }
  void copy() { server.copy(); }

 protected:
  WiFiServer wifi;
  HttpServer server{wifi};
  KARadioProtocol protocol;
  RingBuffer<uint8_t> ringBuffer{512};
  QueueStream<uint8_t> queueStream{ringBuffer};
  Vector<void*> context{1};
  int port = 80;
  const char* ssid = nullptr;
  const char* password = nullptr;

  static void parse(HttpServer* server, const char* requestPath,
                    HttpRequestHandlerLine* hl) {
    LOGI("parse: %s", requestPath);
    KARadioProtocolServer* self = (KARadioProtocolServer*)hl->context[0];
    QueueStream<uint8_t>& queueStream = self->queueStream;
    bool ok = self->protocol.processCommand(requestPath, queueStream);
    server->reply("text/plain", queueStream, queueStream.available(),
                  ok ? 200 : 400, ok ? SUCCESS : "Error");
  }
};
}  // namespace audio_tools