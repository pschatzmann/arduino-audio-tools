#pragma once

#include "AudioConfig.h"
#if defined(USE_AUDIO_SERVER) && (defined(USE_ETHERNET) || defined(USE_WIFI))

#ifdef USE_WIFI
#  ifdef ESP8266
#    include <ESP8266WiFi.h>
#  else
#    include <WiFi.h>
#  endif
#endif

#ifdef USE_ETHERNET
#  include <Ethernet.h>
#endif

#include "AudioTools/AudioCodecs/CodecWAV.h"
#include "AudioTools.h"

namespace audio_tools {

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
template<class Client,class Server>
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
            copier.copy();
          } else {
            copier.copy(*converter_ptr);
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
  void setCopyBufferSize(int size){
    copier.resize(size);
  }

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
    client_obj.println("HTTP/1.1 200 OK");
    LOGI("Reply: HTTP/1.1 200 OK");
    if (content_type != nullptr) {
      client_obj.print("Content-type:");
      client_obj.println(content_type);
      LOGI("Content-type: %s", content_type);
    }
    client_obj.println();
      if (!client_obj.connected()){
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
      if (!client_obj.connected()){
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
      while (client_obj.connected()) {  // loop while the client's connected
        if (client_obj
                .available()) {  // if there's bytes to read from the client,
          char c = client_obj.read();  // read a byte, then
          if (c == '\n') {             // if the byte is a newline character
            LOGI("Request: %s", currentLine.c_str());
            // if the current line is blank, you got two newline characters in a
            // row. that's the end of the client HTTP request, so send a
            // response:
            if (currentLine.length() == 0) {
              sendReplyHeader();
              sendReplyContent();
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

#ifdef USE_WIFI
using AudioServer = AudioServerT<WiFiClient, WiFiServer>;
using AudioServerWiFi = AudioServerT<WiFiClient, WiFiServer>;
#endif

#ifdef USE_ETHERNET
using AudioServer = AudioServerT<EthernetClient, EthernetServer>;
using AudioServerEthernet = AudioServerT<EthernetClient, EthernetServer>;
#endif

/**
 * @brief A simple Arduino Webserver which streams the audio using the indicated
 * encoder.. This class is based on the WiFiServer class. All you need to do is
 * to provide the data with a callback method or from a Stream.
 *
 * @ingroup http
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioEncoderServer : public AudioServer {
 public:
  /**
   * @brief Construct a new Audio Server object that supports an AudioEncoder
   * We assume that the WiFi is already connected
   */
  AudioEncoderServer(AudioEncoder *encoder, int port = 80) : AudioServer(port) {
    this->encoder = encoder;
  }

  /**
   * @brief Construct a new Audio Server object
   *
   * @param network
   * @param password
   */
  AudioEncoderServer(AudioEncoder *encoder, const char *network,
                     const char *password, int port = 80)
      : AudioServer(network, password, port) {
    this->encoder = encoder;
  }

  /**
   * @brief Destructor release the memory
   **/
  ~AudioEncoderServer() {}

  /**
   * @brief Start the server. You need to be connected to WiFI before calling
   * this method
   *
   * @param in
   * @param sample_rate
   * @param channels
   */
  bool begin(Stream &in, int sample_rate, int channels,
             int bits_per_sample = 16, BaseConverter *converter = nullptr) {
    TRACED();
    this->in = &in;
    setConverter(converter);
    audio_info.sample_rate = sample_rate;
    audio_info.channels = channels;
    audio_info.bits_per_sample = bits_per_sample;
    encoder->setAudioInfo(audio_info);
    // encoded_stream.begin(&client_obj, encoder);
    encoded_stream.setOutput(&client_obj);
    encoded_stream.setEncoder(encoder);
    encoded_stream.begin(audio_info);
    return AudioServer::begin(in, encoder->mime());
  }

  /**
   * @brief Start the server. You need to be connected to WiFI before calling
   * this method
   *
   * @param in
   * @param info
   * @param converter
   */
  bool begin(Stream &in, AudioInfo info, BaseConverter *converter = nullptr) {
    TRACED();
    this->in = &in;
    this->audio_info = info;
    setConverter(converter);
    encoder->setAudioInfo(audio_info);
    encoded_stream.setOutput(&client_obj);
    encoded_stream.setEncoder(encoder);
    if (!encoded_stream.begin(audio_info)){
      LOGE("encoder begin failed");
      stop();
    }

    return AudioServer::begin(in, encoder->mime());
  }

  /**
   * @brief Start the server. You need to be connected to WiFI before calling
   * this method
   *
   * @param in
   * @param converter
   */
  bool begin(AudioStream &in, BaseConverter *converter = nullptr) {
    TRACED();
    this->in = &in;
    this->audio_info = in.audioInfo();
    setConverter(converter);
    encoder->setAudioInfo(audio_info);
    encoded_stream.setOutput(&client_obj);
    encoded_stream.setEncoder(encoder);
    encoded_stream.begin(audio_info);

    return AudioServer::begin(in, encoder->mime());
  }

  /**
   * @brief Start the server. The data must be provided by a callback method
   *
   * @param cb
   * @param sample_rate
   * @param channels
   */
  bool begin(AudioServerDataCallback cb, int sample_rate, int channels,
             int bits_per_sample = 16) {
    TRACED();
    audio_info.sample_rate = sample_rate;
    audio_info.channels = channels;
    audio_info.bits_per_sample = bits_per_sample;
    encoder->setAudioInfo(audio_info);

    return AudioServer::begin(cb, encoder->mime());
  }

  // provides a pointer to the encoder
  AudioEncoder *audioEncoder() { return encoder; }

 protected:
  // Sound Generation - use EncodedAudioOutput with is more efficient then EncodedAudioStream
  EncodedAudioOutput encoded_stream;
  AudioInfo audio_info;
  AudioEncoder *encoder = nullptr;

   // moved to be part of reply content to avoid timeout issues in Chrome 
   void sendReplyHeader() override {}

   void sendReplyContent() override {
    TRACED();
    // restart encoder
    if (encoder) {
      encoder->end();
      encoder->begin();
    }

    if (callback != nullptr) {
      // encoded_stream.begin(out_ptr(), encoder);
      encoded_stream.setOutput(out_ptr());
      encoded_stream.setEncoder(encoder);
      encoded_stream.begin();

      // provide data via Callback to encoded_stream
      LOGI("sendReply - calling callback");
      // Send delayed header
      AudioServer::sendReplyHeader();
      callback(&encoded_stream);
      client_obj.stop();
    } else if (in != nullptr) {
      // provide data for stream: in -copy>  encoded_stream -> out
      LOGI("sendReply - Returning encoded stream...");
      // encoded_stream.begin(out_ptr(), encoder);
      encoded_stream.setOutput(out_ptr());
      encoded_stream.setEncoder(encoder);
      encoded_stream.begin();

      copier.begin(encoded_stream, *in);
      if (!client_obj.connected()){
        LOGE("connection was closed");
      }
      // Send delayed header
      AudioServer::sendReplyHeader();
    }
  }
};

/**
 * @brief A simple Arduino Webserver which streams the audio as WAV data.
 * This class is based on the AudioEncodedServer class. All you need to do is to
 * provide the data with a callback method or from a Stream.
 * @ingroup http
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioWAVServer : public AudioEncoderServer {
 public:
  /**
   * @brief Construct a new Audio WAV Server object
   * We assume that the WiFi is already connected
   */
  AudioWAVServer(int port = 80) : AudioEncoderServer(new WAVEncoder(), port) {}

  /**
   * @brief Construct a new Audio WAV Server object
   *
   * @param network
   * @param password
   */
  AudioWAVServer(const char *network, const char *password, int port = 80)
      : AudioEncoderServer(new WAVEncoder(), network, password, port) {}

  /// Destructor: release the allocated encoder
  ~AudioWAVServer() {
    AudioEncoder *encoder = audioEncoder();
    if (encoder != nullptr) {
      delete encoder;
    }
  }

  // provides a pointer to the encoder
  WAVEncoder &wavEncoder() { return *static_cast<WAVEncoder *>(encoder); }
};

}  // namespace audio_tools

#endif
