#pragma once

#include "AudioServerT.h"

namespace audio_tools {

/**
 * @brief A simple Arduino Webserver which streams the audio using the indicated
 * encoder.. This class is based on the WiFiServer class. All you need to do is
 * to provide the data with a callback method or from a Stream.
 *
 * @ingroup http
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <class Client, class Server>
class AudioEncoderServerT : public AudioServerT<Client, Server> {
 public:
  /**
   * @brief Construct a new Audio Server object that supports an AudioEncoder
   * We assume that the WiFi is already connected
   */
  AudioEncoderServerT(AudioEncoder *encoder, int port = 80) : AudioServerT<Client, Server>(port) {
    this->encoder = encoder;
  }

  /**
   * @brief Construct a new Audio Server object
   *
   * @param network
   * @param password
   */
  AudioEncoderServerT(AudioEncoder *encoder, const char *network,
                     const char *password, int port = 80)
      : AudioServerT<Client, Server>(network, password, port) {
    this->encoder = encoder;
  }

  /**
   * @brief Destructor release the memory
   **/
  virtual ~AudioEncoderServerT() = default;

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
    AudioServerT<Client, Server>::setConverter(converter);
    audio_info.sample_rate = sample_rate;
    audio_info.channels = channels;
    audio_info.bits_per_sample = bits_per_sample;
    encoder->setAudioInfo(audio_info);
    // encoded_stream.begin(&client_obj, encoder);
    encoded_stream.setOutput(&this->client_obj);
    encoded_stream.setEncoder(encoder);
    encoded_stream.begin(audio_info);
    return AudioServerT<Client, Server>::begin(in, encoder->mime());
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
    AudioServerT<Client, Server>::setConverter(converter);
    encoder->setAudioInfo(audio_info);
    encoded_stream.setOutput(&this->client_obj);
    encoded_stream.setEncoder(encoder);
    if (!encoded_stream.begin(audio_info)) {
      LOGE("encoder begin failed");
      // stop();
    }

    return AudioServerT<Client, Server>::begin(in, encoder->mime());
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
    AudioServerT<Client, Server>::setConverter(converter);
    encoder->setAudioInfo(audio_info);
    encoded_stream.setOutput(&this->client_obj);
    encoded_stream.setEncoder(encoder);
    encoded_stream.begin(audio_info);

    return AudioServerT<Client, Server>::begin(in, encoder->mime());
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

    return AudioServerT<Client, Server>::begin(cb, encoder->mime());
  }

  // provides a pointer to the encoder
  AudioEncoder *audioEncoder() { return encoder; }

 protected:
  // Sound Generation - use EncodedAudioOutput with is more efficient then
  // EncodedAudioStream
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

    if (this->callback != nullptr) {
      // encoded_stream.begin(out_ptr(), encoder);
      encoded_stream.setOutput(this->out_ptr());
      encoded_stream.setEncoder(encoder);
      encoded_stream.begin();

      // provide data via Callback to encoded_stream
      LOGI("sendReply - calling callback");
      // Send delayed header
      AudioServerT<Client, Server>::sendReplyHeader();
      this->callback(&encoded_stream);
      this->client_obj.stop();
    } else if (this->in != nullptr) {
      // provide data for stream: in -copy>  encoded_stream -> out
      LOGI("sendReply - Returning encoded stream...");
      // encoded_stream.begin(out_ptr(), encoder);
      encoded_stream.setOutput(this->out_ptr());
      encoded_stream.setEncoder(encoder);
      encoded_stream.begin();

      this->copier.begin(encoded_stream, *this->in);
      if (!this->client_obj.connected()) {
        LOGE("connection was closed");
      }
      // Send delayed header
      AudioServerT<Client, Server>::sendReplyHeader();
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
template <class Client, class Server>
class AudioWAVServerT : public AudioEncoderServerT<Client, Server> {
 public:
  /**
   * @brief Construct a new Audio WAV Server object
   * We assume that the WiFi is already connected
   */
  AudioWAVServerT(int port = 80) : AudioEncoderServerT<Client, Server>(new WAVEncoder(), port) {}

  /**
   * @brief Construct a new Audio WAV Server object
   *
   * @param network
   * @param password
   */
  AudioWAVServerT(const char *network, const char *password, int port = 80)
      : AudioEncoderServerT<Client, Server>(new WAVEncoder(), network, password, port) {}

  /// Destructor: release the allocated encoder
  ~AudioWAVServerT() {
    AudioEncoder *encoder = AudioEncoderServerT<Client, Server>::audioEncoder();
    if (encoder != nullptr) {
      delete encoder;
    }
  }

  // provides a pointer to the encoder
  WAVEncoder &wavEncoder() { return *static_cast<WAVEncoder *>(AudioEncoderServerT<Client, Server>::encoder); }
};



} // namespace audio_tools

