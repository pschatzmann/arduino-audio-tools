
#pragma once

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/CoreAudio/AudioBasic/StrView.h"
#include "AudioTools/CoreAudio/AudioMetaData/MimeDetector.h"

namespace audio_tools {

/**
 * @brief Manage multiple decoders: the actual decoder is only opened when it
 * has been selected. The relevant decoder is determined dynamically at the
 * first write from the determined mime type. You can add your own custom mime
 * type determination logic.
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MultiDecoder : public AudioDecoder {
 public:
  MultiDecoder() = default;
  MultiDecoder(AbstractURLStream& url) { setMimeSource(url); }

  /// Enables the automatic mime type determination
  bool begin() override {
    mime_detector.begin();
    is_first = true;
    return true;
  }

  /// closes the actual decoder
  void end() override {
    if (actual_decoder.decoder != nullptr && actual_decoder.is_open) {
      actual_decoder.decoder->end();
    }
    actual_decoder.is_open = false;
    actual_decoder.decoder = nullptr;
    actual_decoder.mime = nullptr;
    is_first = true;
  }

  /// Adds a decoder that will be selected by it's mime type
  void addDecoder(AudioDecoder& decoder, const char* mime) {
    DecoderInfo info{mime, &decoder};
    decoder.addNotifyAudioChange(*this);
    decoders.push_back(info);
  }

  /// Adds a decoder that will be selected by it's mime type and defines the
  /// mime checking logic
  void addDecoder(AudioDecoder& decoder, const char* mime,
                  bool (*check)(uint8_t* data, size_t len)) {
    addDecoder(decoder, mime);
    mime_detector.setCheck(mime, check);
  }

  virtual void setOutput(Print& out_stream) override {
    p_print = &out_stream;
    for (int j = 0; j < decoders.size(); j++) {
      decoders[j].decoder->setOutput(out_stream);
    }
  }

  /// Defines url stream from which we determine the mime type from the reply header
  void setMimeSource(AbstractURLStream& url) { p_url_stream = &url; }

  /// selects the actual decoder by mime type - this is usually called
  /// automatically from the determined mime type
  bool selectDecoder(const char* mime) {
    bool result = false;
    // do nothing if no change
    if (StrView(mime).equals(actual_decoder.mime)) {
      is_first = false;
      return true;
    }
    // close actual decoder
    end();

    // find the corresponding decoder
    for (int j = 0; j < decoders.size(); j++) {
      DecoderInfo info = decoders[j];
      if (StrView(info.mime).equals(mime)) {
        LOGI("New decoder found for %s (%s)", info.mime, mime);
        actual_decoder = info;
        // define output if it has not been defined
        if (p_print != nullptr &&
            actual_decoder.decoder->getOutput() == nullptr) {
          actual_decoder.decoder->setOutput(*p_print);
        }
        actual_decoder.decoder->begin();
        result = true;
      }
    }
    is_first = false;
    return result;
  }

  size_t write(const uint8_t* data, size_t len) override {
    if (is_first) {
      const char* mime = nullptr;
      if (p_url_stream != nullptr) {
        // get content type from http header
        mime = p_url_stream->getReplyHeader(CONTENT_TYPE);
        if (mime) LOGI("mime from http request: %s", mime);
      }
      if (mime == nullptr) {
        // use the mime detector
        mime_detector.write((uint8_t*)data, len);
        mime = mime_detector.mime();
        if (mime) LOGI("mime from mime_detector: %s", mime);
      }
      if (mime != nullptr) {
        // select the decoder based on the detemined mime type
        if (!selectDecoder(mime)) {
          LOGE("The decoder could not be found for %s", mime);
          actual_decoder.decoder = &nop;
          actual_decoder.is_open = true;
        }
      }
      is_first = false;
    }
    // check if we have a decoder
    if (actual_decoder.decoder == nullptr) return 0;
    // decode the data
    return actual_decoder.decoder->write(data, len);
  }

  virtual operator bool() {
    if (actual_decoder.decoder == &nop) return false;
    return is_first || actual_decoder.is_open;
  };

 protected:
  struct DecoderInfo {
    const char* mime = nullptr;
    AudioDecoder* decoder = nullptr;
    bool is_open = false;
    DecoderInfo() = default;
    DecoderInfo(const char* mime, AudioDecoder* decoder) {
      this->mime = mime;
      this->decoder = decoder;
    }
  } actual_decoder;
  Vector<DecoderInfo> decoders{0};
  MimeDetector mime_detector;
  CodecNOP nop;
  AbstractURLStream* p_url_stream = nullptr;
  bool is_first = true;
};

}  // namespace audio_tools