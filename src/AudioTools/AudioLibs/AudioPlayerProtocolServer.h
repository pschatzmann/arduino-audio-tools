#pragma once
#include "AudioPlayerProtocol.h"
#include "AudioTools/CoreAudio/BaseStream.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "HttpServer.h"

namespace audio_tools {

/***
 * @brief Audio Player Protocol Server: We can use the indicated protocol over
 * http to control the audio player provided by the audiotools.
 * @author Phil Schatzmann
 */
class AudioPlayerProtocolServer {
 public:
  /// Default constructor
  AudioPlayerProtocolServer(AudioPlayerProtocol& protocol,
                                          AudioPlayer& player, int port = 80,
                                          const char* ssid = nullptr,
                                          const char* pwd = nullptr) {
    setProtocol(protocol);
    setPlayer(player);
    setPort(port);
    setSSID(ssid);
    setPassword(pwd);
  }

  /// Empty constructor: call setPlayer to define the player
  AudioPlayerProtocolServer() = default;

  /// Defines the player
  void setPlayer(AudioPlayer& player) { p_protocol->setPlayer(player); }

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

  /// Defines the buffer size that is made available for the http reply
  void setBufferSize(int size) { buffer_size = size; }

  void setProtocol(AudioPlayerProtocol& protocol) {
    this->p_protocol = &protocol;
  }

 protected:
  WiFiServer wifi;
  HttpServer server{wifi};
  AudioPlayerProtocol* p_protocol;
  RingBuffer<uint8_t> ringBuffer{0};
  QueueStream<uint8_t> queueStream{ringBuffer};
  Vector<void*> context{1};
  int port = 80;
  const char* ssid = nullptr;
  const char* password = nullptr;
  int buffer_size = 512;

  static void parse(HttpServer* server, const char* requestPath,
                    HttpRequestHandlerLine* hl) {
    LOGI("parse: %s", requestPath);
    AudioPlayerProtocolServer* self = (AudioPlayerProtocolServer*)hl->context[0];
    self->ringBuffer.resize(self->buffer_size);
    QueueStream<uint8_t>& queueStream = self->queueStream;
    queueStream.begin();
    bool ok = self->p_protocol->processCommand(requestPath, queueStream);
    LOGI("available: %d", queueStream.available());
    server->reply("text/plain", queueStream, queueStream.available(),
                  ok ? 200 : 400, ok ? SUCCESS : "Error");
    self->ringBuffer.resize(0);
  }
};

}  // namespace audio_tools