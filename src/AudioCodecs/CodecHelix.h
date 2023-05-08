#pragma once

#include "AudioCodecs/AudioEncoded.h"
#include "AudioCodecs/CodecWAV.h"
#include "AudioCodecs/CodecMP3Helix.h"
#include "AudioCodecs/CodecAACHelix.h"

namespace audio_tools {

/**
 * @brief MP3 and AAC Decoder using libhelix:
 * https://github.com/pschatzmann/arduino-libhelix. We dynamically create a MP3 or AAC decoder dependent on the provided audio format.
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class DecoderHelix : public AudioDecoder {
public:
  DecoderHelix() { TRACED(); }

  DecoderHelix(Print &out_stream) {
    TRACED();
    p_out_stream = &out_stream;
  }

  DecoderHelix(Print &out_stream, AudioInfoSupport &bi) {
    TRACED();
    p_out_stream = &out_stream;
    p_bi = &bi;
  }

  ~DecoderHelix() { resetDecoder(); }

  /// Defines the output Stream
  virtual void setOutput(Print &outStream) { p_out_stream = &outStream; }

  /// Starts the processing
  void begin() {
    TRACED();
    // reset actual decoder so that we start a new determination
    resetDecoder();
  }

  /// Releases the reserved memory
  void end() {
    TRACED();
    if (p_decoder!=nullptr){
      p_decoder->end();
    }
    resetDecoder();
  }

  AudioInfo audioInfo() {
    return p_decoder != nullptr ? p_decoder->audioInfo() : noInfo;
  }

  /// Write mp3 data to decoder
  size_t write(const void *data, size_t len) {
    LOGD("%s: %zu", LOG_METHOD, len);
    if (p_decoder == nullptr) {
      setupDecoder((const byte *)data);
      p_decoder->begin();
    }
    return p_decoder->write(data, len);
  }

  /// checks if the class is active
  operator bool() { return p_decoder == nullptr ? false : *p_decoder; }

  /// Defines the callback object to which the Audio information change is
  /// provided
  void setNotifyAudioChange(AudioInfoSupport &bi) { p_bi = &bi; }

protected:
  AudioDecoder *p_decoder = nullptr;
  Print *p_out_stream = nullptr;
  AudioInfoSupport *p_bi = nullptr;
  AudioInfo noInfo;

  /// Defines the decoder based on the audio format
  void setupDecoder(const byte *start) {
    if (start[0] == 0xFF && start[1] == 0xF1) {
      p_decoder = new AACDecoderHelix();
      LOGI("using AACDecoderHelix");
    } else if (start[0] == 0xFF || start[0] == 0xFE || strncmp("ID3", (const char*)start, 3)==0) {
      p_decoder = new MP3DecoderHelix();
      LOGI("using MP3DecoderHelix");
    } else if (strncmp("RIFF", (const char*)start, 4)==0) {
      p_decoder = new WAVDecoder();
      LOGI("using WAVDecoder");
    }
    // if we do not have a decoder yet we use a dummy to prevent NPE
    if (p_decoder == nullptr) {
      LOGW("Unknown Data Format: Content will be ignored...")
      p_decoder = CodecNOP::instance();
    }
    p_decoder->setOutput(*p_out_stream);
    p_decoder->setNotifyAudioChange(*p_bi);
  }

  /// Deletes the decoder
  void resetDecoder() {
    if (p_decoder != nullptr) {
      delete p_decoder;
    }
    p_decoder = nullptr;
  }
};

} // namespace audio_tools

