#pragma once

#include "AudioCodecs/CodecOgg.h"
#include "AudioCodecs/AudioEncoded.h"
#include "ivorbiscodec.h"
#include "ivorbisfile.h"

namespace audio_tools {

/**
 * @brief Vorbis Streaming Decoder using
 * https://github.com/pschatzmann/arduino-libvorbis-idec
 * https://github.com/pschatzmann/arduino-libopus.git
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VorbisDecoderTremor : public StreamingDecoder {
public:
  VorbisDecoderTremor() = default;

  /// Destroy the VorbisDecoderTremor object
  ~VorbisDecoderTremor() {
    if (active) {
      end();
    }
  }

  void setNotifyAudioChange(AudioBaseInfoDependent &bi) override {
    p_notify = &bi;
  }

  /// Starts the processing
  void begin() override {
    LOGD(LOG_METHOD);

    callbacks.read_func = read_func;
    callbacks.seek_func = seek_func;
    callbacks.close_func = close_func;
    callbacks.tell_func = tell_func;

    int rc = ov_open_callbacks(this, &file, nullptr, 0, callbacks);

    pcm.resize(1024);
    active = true;
  }

  /// Releases the reserved memory
  void end() override {
    LOGD(LOG_METHOD);
    active = false;
    ov_clear(&file);
  }

  /// Defines the output Stream
  void setOutputStream(Print &outStream) override { this->p_out = &outStream; }

  /// Defines the output Stream
  void setInputStream(Stream &inStream) override { this->p_in = &inStream; }

  /// Provides the last available MP3FrameInfo
  AudioBaseInfo audioInfo() override { return cfg; }

  /// checks if the class is active
  virtual operator boolean() override { return active; }

  virtual long copy() override {
    long result = ov_read(&file, (char *)pcm.data(), pcm.size(), &bitstream);
    if (result > 0) {
      AudioBaseInfo current = currentInfo();
      if (current != cfg) {
        cfg = current;
        p_notify->setAudioInfo(cfg);
      }
      p_out->write(pcm.data(), result);
    }
    return result;
  }

protected:
  AudioBaseInfo cfg;
  AudioBaseInfoDependent *p_notify = nullptr;
  Print *p_out = nullptr;
  Stream *p_in = nullptr;
  Vector<uint8_t> pcm;
  OggVorbis_File file;
  ov_callbacks callbacks;
  bool active;
  int bitstream;

  AudioBaseInfo currentInfo() {
    AudioBaseInfo result;
    vorbis_info *info = ov_info(&file, -1);
    result.sample_rate = info->rate;
    result.channels = info->channels;
    result.bits_per_sample = 2;
    return result;
  }

  static size_t read_func(void *ptr, size_t size, size_t nmemb,
                          void *datasource) {
    VorbisDecoderTremor *self = (VorbisDecoderTremor *)datasource;
    return self->p_in->readBytes((uint8_t *)ptr, size);
  }

  static int seek_func(void *datasource, ogg_int64_t offset, int whence) {
    VorbisDecoderTremor *self = (VorbisDecoderTremor *)datasource;
    return -1;
  }

  static long tell_func(void *datasource) {
    VorbisDecoderTremor *self = (VorbisDecoderTremor *)datasource;
    return -1;
  }

  static int close_func(void *datasource) {
    VorbisDecoderTremor *self = (VorbisDecoderTremor *)datasource;
    self->end();
    return 0;
  }
};

} // namespace audio_tools
