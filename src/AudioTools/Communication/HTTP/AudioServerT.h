#pragma once

#include "AudioTools/AudioCodecs/CodecWAV.h"
#include "AudioTools/CoreAudio/AudioStreams.h"
#include "AudioTools/CoreAudio/StreamCopy.h"
#include "AudioToolsConfig.h"

namespace audio_tools {

/// Calback which writes the sound data to the stream
typedef void (*AudioServerDataCallback)(Print *out);

/**
 * @brief A simple Arduino Webserver template which streams the result
 * This template class can work with different Client and Server types.
 * All you need to do is to provide the data with a callback method or
 * from an Arduino Stream: in -copy> client
 *
 * @ingroup http
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

/// Calback which writes the sound data to the stream
typedef void (*AudioServerDataCallback)(Print *out);

/**
 * @brief A simple Arduino Webserver which streams the result
 * This class is based on the WiFiServer class. All you need to do is to provide
 * the data with a callback method or from an Arduino Stream:   in -copy> client
 *
 * @ingroup http
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <class Client, class Server>
class AudioServerT {
 public:
  /**
   * @brief Construct a new Audio Server object
   * We assume that the WiFi is already connected
   */
  AudioServerT(int port = 80) {
    // the client returns 0 for avialableForWrite()
    copier.setCheckAvailableForWrite(false);
    setupServer(port);
  }

  /**
   * @brief Construct a new Audio WAV Server object
   *
   * @param network
   * @param password
   */
  AudioServerT(const char *network, const char *password, int port = 80) {
    this->network = (char *)network;
    this->password = (char *)password;
    // the client returns 0 for avialableForWrite()
    copier.setCheckAvailableForWrite(false);
    setupServer(port);
  }

  /**
   * @brief Start the server. You need to be connected to WiFI before calling
   * this method
   *
   * @param in
   * @param contentType Mime Type of result
   */
  bool begin(Stream &in, const char *contentType) {
    TRACED();
    this->in = &in;
    this->content_type = contentType;

#ifdef USE_WIFI
    connectWiFi();
#endif
    // start server
    server.begin();
    return true;
  }

  /**
   * @brief Start the server. The data must be provided by a callback method
   *
   * @param cb
   * @param contentType Mime Type of result
   */
  bool begin(AudioServerDataCallback cb, const char *contentType) {
    TRACED();
    this->in = nullptr;
    this->callback = cb;
    this->content_type = contentType;

#ifdef USE_WIFI
    connectWiFi();
#endif

    // start server
    server.begin();
    return true;
  }

  /**
   * @brief Add this method to your loop
   * Returns true while the client is connected. (The same functionality like
   * doLoop())
   *
   * @return true
   * @return false
   */
  bool copy() { return doLoop(); }

  /**
   * @brief Add this method to your loop
   * Returns true while the client is connected.
   */
  bool doLoop() {
    // LOGD("doLoop");
    bool active = true;
    if (!client_obj.connected()) {
#if USE_SERVER_ACCEPT
      client_obj = server.accept();  // listen for incoming clients
#else
      client_obj = server.available();  // listen for incoming clients
#endif
      processClient();
    } else {
      // We are connected: copy input from source to wav output
      if (client_obj) {
        if (callback == nullptr) {
          LOGD("copy data...");
          if (converter_ptr == nullptr) {
            sent += copier.copy();
          } else {
            sent += copier.copy(*converter_ptr);
          }

          if (max_bytes > 0 && sent >= max_bytes) {
            LOGI("range exhausted...");
            client_obj.stop();
            active = false;
          }

          // if we limit the size of the WAV the encoder gets automatically
          // closed when all has been sent
          if (!client_obj) {
            LOGI("stop client...");
            client_obj.stop();
            active = false;
          }
        }
      } else {
        LOGI("client was not connected");
      }
    }
    return active;
  }

  /// defines a converter that will be used when the audio is rendered
  void setConverter(BaseConverter *c) { converter_ptr = c; }

  /// Provides the output stream
  Stream &out() { return client_obj; }

  /// Provides a pointer to the WiFiClient
  Client *out_ptr() { return &client_obj; }

  /// Checks if any clinent has connnected
  bool isClientConnected() { return client_obj.connected(); }

  /// Changes the copy buffer size
  void setCopyBufferSize(int size) { copier.resize(size); }

 protected:
  // WIFI
#ifdef ESP32
  Server server;
#else
  Server server{80};
#endif
  Client client_obj;
  char *password = nullptr;
  char *network = nullptr;
  size_t max_bytes = 0;
  size_t sent = 0;

  // Content
  const char *content_type = nullptr;
  AudioServerDataCallback callback = nullptr;
  Stream *in = nullptr;
  StreamCopy copier;
  BaseConverter *converter_ptr = nullptr;

  void setupServer(int port) {
    Server tmp(port);
    server = tmp;
  }

#ifdef USE_WIFI
  void connectWiFi() {
    TRACED();
    if (WiFi.status() != WL_CONNECTED && network != nullptr &&
        password != nullptr) {
      WiFi.begin(network, password);
      while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
      }
#ifdef ESP32
      WiFi.setSleep(false);
#endif
      Serial.println();
    }
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
#endif

  virtual void sendReplyHeader() {
    TRACED();

    // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
    // and a content-type so the client knows what's coming, then a blank line:
    const char *response;
    if (max_bytes > 0) {
      response = "HTTP/1.1 206 OK";
    } else {
      response = "HTTP/1.1 200 OK";
    }
    client_obj.println(response);
    LOGI("%s", response);
    if (content_type != nullptr) {
      client_obj.print("Content-type:");
      client_obj.println(content_type);
      LOGI("Content-type: %s", content_type);
    }
    client_obj.println();
    if (!client_obj.connected()) {
      LOGE("connection was closed");
    }
  }

  virtual void sendReplyContent() {
    TRACED();
    if (callback != nullptr) {
      // provide data via Callback
      LOGI("sendReply - calling callback");
      callback(&client_obj);
      client_obj.stop();
    } else if (in != nullptr) {
      // provide data for stream
      LOGI("sendReply - Returning audio stream...");
      copier.begin(client_obj, *in);
      if (!client_obj.connected()) {
        LOGE("connection was closed");
      }
    }
  }

  // Handle an new client connection and return the data
  void processClient() {
    // LOGD("processClient");
    if (client_obj) {       // if you get a client,
      LOGI("New Client:");  // print a message out the serial port
      String currentLine =
          "";  // make a String to hold incoming data from the client
      long firstbyte = 0;
      long lastbyte = 0;
      while (client_obj.connected()) {  // loop while the client's connected
        if (client_obj
                .available()) {  // if there's bytes to read from the client,
          char c = client_obj.read();  // read a byte, then
          if (c == '\n') {             // if the byte is a newline character
            LOGI("Request: %s", currentLine.c_str());
            if (currentLine.startsWith(String("Range: bytes="))) {
              int minuspos = currentLine.indexOf('-', 13);

              // toInt returns 0 if it's an invalid conversion, so it's "safe"
              firstbyte = currentLine.substring(13, minuspos).toInt();
              lastbyte = currentLine.substring(minuspos + 1).toInt();
            }
            // if the current line is blank, you got two newline characters in a
            // row. that's the end of the client HTTP request, so send a
            // response:
            if (currentLine.length() == 0) {
              sendReplyHeader();
              sendReplyContent();
              max_bytes = lastbyte - firstbyte;
              sent = 0;
              // break out of the while loop:
              break;
            } else {  // if you got a newline, then clear currentLine:
              currentLine = "";
            }
          } else if (c != '\r') {  // if you got anything else but a carriage
                                   // return character,
            currentLine += c;      // add it to the end of the currentLine
          }
        }
      }
    }
  }
};

}  // namespace audio_tools
