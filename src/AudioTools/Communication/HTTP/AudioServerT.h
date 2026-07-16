#pragma once

#include <string>

#include "AudioTools/AudioCodecs/CodecWAV.h"
#include "AudioTools/CoreAudio/AudioStreams.h"
#include "AudioTools/CoreAudio/StreamCopy.h"
#include "AudioToolsConfig.h"

namespace audio_tools {

/// Callback which writes the sound data to the stream
typedef void (*AudioServerDataCallback)(Print *out);

/**
 * @brief Wraps a Print target and applies HTTP chunked transfer encoding
 * framing (chunk-size in hex + CRLF + data + CRLF) to everything that is
 * written to it.
 * see https://en.wikipedia.org/wiki/Chunked_transfer_encoding
 *
 * @ingroup http
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class ChunkedPrint : public Print {
 public:
  ChunkedPrint() = default;

  /// (re-)starts the chunked encoding for the indicated target
  void begin(Print &out) { p_out = &out; }

  size_t write(uint8_t c) override { return write(&c, 1); }

  size_t write(const uint8_t *data, size_t len) override {
    if (p_out == nullptr || len == 0) return 0;
    p_out->print(len, HEX);
    p_out->print("\r\n");
    size_t written = p_out->write(data, len);
    p_out->print("\r\n");
    return written;
  }

  /// Sends the terminating (zero length) chunk
  void end() {
    if (p_out == nullptr) return;
    p_out->print("0\r\n\r\n");
    p_out = nullptr;
  }

 protected:
  Print *p_out = nullptr;
};

/**
 * @brief A minimal, single-client HTTP server that streams audio (or any
 * other data) to a browser or media player: in -copy> client. The data is
 * provided either from an Arduino Stream or via an AudioServerDataCallback,
 * and is copied to the connected client on each doLoop()/copy() call, so no
 * threads or blocking loops are used.
 *
 * The class is templated on the Client/Server implementation (e.g.
 * WiFiClient/WiFiServer, EthernetClient/EthernetServer) so it can run on top
 * of any Arduino networking stack that follows that API.
 *
 * By default the response is sent with `Transfer-Encoding: chunked` since
 * the total size is usually not known upfront; call setMaxOutputSize() if
 * the size is known to send a `Content-Length` header instead.
 *
 * This class only serves the data - it does not do any audio encoding. Use
 * AudioEncoderServerT if you need the input to be encoded (e.g. to WAV or
 * MP3) before it is sent to the client.
 *
 * @tparam Client the client class e.g. WiFiClient
 * @tparam Server the server class e.g. WiFiServer
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
    // the client returns 0 for availableForWrite()
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
    this->network = network ? network : "";
    this->password = password ? password : "";

    // the client returns 0 for availableForWrite()
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
    this->callback = nullptr;
    setContentType(contentType);

    return startServer();
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
    setContentType(contentType);

    return startServer();
  }

  /// Just starts the server
  bool begin() {
    if (content_type == nullptr) {
      LOGE("Content type is not defined");
      return false;
    }
    return startServer();
  }

  /// Defines the content type (MIME type) of the data that will be sent to the client. Needs to be set before calling begin().
  void setContentType(const char *contentType) { this->content_type = contentType ? contentType : ""; }

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

          if (max_output_size > 0 && sent >= max_output_size) {
            LOGI("max output size reached...");
            endChunked();
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

  /// Checks if any client has connected
  bool isClientConnected() { return client_obj.connected(); }

  /// Checks if any client has connected
  operator bool() { return client_obj.connected(); }

  /// Changes the copy buffer size
  void setCopyBufferSize(int size) { copier.resize(size); }

  /// Activates/deactivates HTTP chunked transfer encoding (Transfer-Encoding:
  /// chunked) instead of relying on the connection close to signal the end
  /// of the data. Needs to be set before calling begin().
  void setChunked(bool flag) { is_chunked = flag; }

  /// Determines if chunked transfer encoding is active
  bool isChunked() { return is_chunked; }

  /// Defines the total number of bytes that will be sent so that a
  /// Content-Length header can be provided. If no size is defined (default),
  /// the size is unknown up front, so we fall back to chunked transfer
  /// encoding instead. Needs to be set before calling begin().
  void setMaxOutputSize(size_t size) {
    max_output_size = size;
    is_chunked = (size == 0);
  }

  /// Provides the defined total number of bytes that will be sent
  size_t maxOutputSize() { return max_output_size; }

  /// Ends the connection and resets the state
  void end() {
    if (client_obj) {
      endChunked();
      client_obj.stop();
    }
    // reset the state
    callback = nullptr;
    in = nullptr;
    converter_ptr = nullptr;
    sent = 0;
  }


 protected:
  // WIFI
#ifdef ESP32
  Server server;
#else
  Server server{80};
#endif

  Client client_obj;

  std::string password;
  std::string network;

  size_t sent = 0;
  size_t max_output_size = 0;

  // Content
  const char *content_type = nullptr;

  AudioServerDataCallback callback = nullptr;
  Stream *in = nullptr;

  StreamCopy copier;
  BaseConverter *converter_ptr = nullptr;

  // no size defined (default) => fall back to chunked transfer encoding
  bool is_chunked = true;
  ChunkedPrint chunked_print;

  void setupServer(int port) {
    Server tmp(port);
    server = tmp;
  }

  /// Connects to WiFi (if enabled) and starts the server: shared tail of all
  /// begin() overloads
  bool startServer() {
#ifdef USE_WIFI
    connectWiFi();
#endif
    server.begin();
    return true;
  }

#ifdef USE_WIFI
  void connectWiFi() {
    TRACED();

    if (WiFi.status() != WL_CONNECTED &&
        !network.empty() &&
        !password.empty()) {

      WiFi.begin(network.c_str(), password.c_str());

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
    const char *response = "HTTP/1.1 200 OK";
    client_obj.println(response);
    LOGI("%s", response);

    if (content_type != nullptr) {
      client_obj.print("Content-type:");
      client_obj.println(content_type);
      LOGI("Content-type: %s", content_type);
    }

    // for firefox to play
    client_obj.println("Content-Disposition: inline");

    if (max_output_size > 0) {
      client_obj.print("Content-Length:");
      client_obj.println((unsigned long)max_output_size);
      LOGI("Content-Length: %lu", (unsigned long)max_output_size);
    } else if (is_chunked) {
      client_obj.println("Transfer-Encoding: chunked");
      LOGI("Transfer-Encoding: chunked");
    }

    client_obj.println();

    if (!client_obj.connected()) {
      LOGE("connection was closed");
    }
  }

  virtual void sendReplyContent() {
    TRACED();

    Print *p_out = &client_obj;
    if (is_chunked) {
      chunked_print.begin(client_obj);
      p_out = &chunked_print;
    }

    if (callback != nullptr) {
      // provide data via Callback
      LOGI("sendReply - calling callback");
      callback(p_out);
      endChunked();
      client_obj.stop();
    } else if (in != nullptr) {
      // provide data for stream
      LOGI("sendReply - Returning audio stream...");
      copier.begin(*p_out, *in);

      if (!client_obj.connected()) {
        LOGE("connection was closed");
      }
    }
  }

  /// Sends the terminating 0-length chunk when chunked transfer is active
  void endChunked() {
    if (is_chunked) chunked_print.end();
  }

  /**
   * @brief Handle an new client connection and return the data
   */
  void processClient() {
    // LOGD("processClient");
    if (client_obj) {
      LOGI("New Client:");

      std::string currentLine;
      currentLine.reserve(128);

      while (client_obj.connected()) {
        if (client_obj.available()) {
          char c = client_obj.read();

          if (c == '\n') {
            LOGI("Request: %s", currentLine.c_str());

            // if the current line is blank, you got two newline characters in a
            // row. that's the end of the client HTTP request, so send a response:
            if (currentLine.length() == 0) {
              sendReplyHeader();
              sendReplyContent();

              sent = 0;

              break;
            } else {
              currentLine = "";
            }
          } else if (c != '\r') {
            currentLine += c;
          }
        }
      }
    }
  }
};

}  // namespace audio_tools