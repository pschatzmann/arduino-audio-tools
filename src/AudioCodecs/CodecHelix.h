#pragma once

#if defined(USE_HELIX) || defined(USE_DECODERS)

#include "AACDecoderHelix.h"
#include "AudioTools/AudioTypes.h"
#include "CodecNOP.h"
#include "CodecWAV.h"
#include "MP3DecoderHelix.h"

namespace audio_tools {

/**
 * @brief MP3 and AAC Decoder using libhelix:
 * https://github.com/pschatzmann/arduino-libhelix
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class DecoderHelix : public AudioDecoder {
public:
  DecoderHelix() { LOGD(LOG_METHOD); }

  DecoderHelix(Print &out_stream) {
    LOGD(LOG_METHOD);
    p_out_stream = &out_stream;
  }

  DecoderHelix(Print &out_stream, AudioBaseInfoDependent &bi) {
    LOGD(LOG_METHOD);
    p_out_stream = &out_stream;
    p_bi = &bi;
  }
  ~MP3DecoderHelix() { resetDecoder(); }

  /// Defines the output Stream
  virtual void setOutputStream(Print &outStream) { p_out_stream = &out_stream; }

  /// Starts the processing
  void begin() {
    LOGD(LOG_METHOD);
    // reset actual decoder so that we start a new determination
    resetDecoder();
  }

  /// Releases the reserved memory
  void end() {
    LOGD(LOG_METHOD);
    p_decoder->end();
    resetDecoder();
  }

  AudioBaseInfo audioInfo() {
    return p_decoder != nullptr ? p_decoder->audioInfo() : noInfo;
  }

  /// Write mp3 data to decoder
  size_t write(const void *data, size_t len) {
    LOGD("%s: %zu", LOG_METHOD, len);
    if (p_decoder == nullptr) {
      setupDecoder();
      p_decoder->begin();
    }
    p_decoder->write(data, len);
  }

  /// checks if the class is active
  operator boolean() { return p_decoder == nullptr ? false : *p_decoder; }

  /// Defines the callback object to which the Audio information change is
  /// provided
  void setNotifyAudioChange(AudioBaseInfoDependent &bi) { p_bi = &bi; }

protected:
  AudioDecoder *p_decoder = nullptr;
  Print *out_stream = nullptr;
  AudioBaseInfoDependent *p_bi = nullptr;
  AudioBaseInfo noInfo;

  void setupDecoder() {
    if (start[0] == 0xFF && start[1] == 0xF1) {
      p_decoder = new AACDecoderHelix();
    } else if (strncmp(start, "ID3", 3) || start[0] == 0xFF ||
               start[0] == 0xFE) {
      p_decoder = new MP3DecoderHelix();
    } else if (strncmp(start, "RIFF", 4)) {
      p_decoder = new WAVDecoder();
    }
    // if we do not have a decoder yet we use a dummy to prevent NPE
    if (p_decoder == nullptr) {
      LOGW("Unknown Data Format: Content will be ignored...")
      p_decoder = new DecoderNOP();
    }
    p_decoder->setOutputStream(*out_stream);
    p_decoder->setNotifyAudioChange(bi);
  }

  void resetDecoder() {
    if (p_decoder != nullptr) {
      delete p_decoder;
    }
    p_decoder = nullptr;
  }
};

} // namespace audio_tools

#endif
