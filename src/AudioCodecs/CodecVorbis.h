#pragma once

#include "AudioCodecs/ContainerOgg.h"
#include "AudioCodecs/AudioEncoded.h"
#include "ivorbiscodec.h"
#include "ivorbisfile.h"


namespace audio_tools {

#ifndef VARBIS_MAX_READ_SIZE
#define VARBIS_MAX_READ_SIZE 512
#endif

/**
 * @brief Vorbis Streaming Decoder using
 * https://github.com/pschatzmann/arduino-libvorbis-idec
 * https://github.com/pschatzmann/arduino-libopus.git
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VorbisDecoder : public StreamingDecoder {
public:
  VorbisDecoder() = default;

  /// Destroy the VorbisDecoder object
  ~VorbisDecoder() {
    if (active) {
      end();
    }
  }

  void setNotifyAudioChange(AudioInfoSupport &bi) override {
    p_notify = &bi;
  }

  /// Starts the processing
  void begin() override {
    LOGI("begin");

    callbacks.read_func = read_func;
    callbacks.seek_func = seek_func;
    callbacks.close_func = close_func;
    callbacks.tell_func = tell_func;

    int rc = ov_open_callbacks(this, &file, nullptr, 0, callbacks);

    pcm.resize(VARBIS_MAX_READ_SIZE);

    active = true;
    is_first = true;
  }

  /// Releases the reserved memory
  void end() override {
    LOGI("end");
    active = false;
    ov_clear(&file);
  }

  /// Defines the output Stream
  void setOutput(Print &outStream) override { this->p_out = &outStream; }

  /// Defines the output Stream
  void setInputStream(Stream &inStream) override { this->p_in = &inStream; }

  /// Provides the last available MP3FrameInfo
  AudioInfo audioInfo() override { return cfg; }

  /// checks if the class is active
  virtual operator bool() override { return active; }

  virtual bool copy() override {
    LOGI("copy");

    if (is_first){
          // wait for some data
      while(p_in->available()==0){
        delay(1);
      }
      is_first = false;
    }


    long result = ov_read(&file, (char *)pcm.data(), pcm.size(), &bitstream);
    if (result > 0) {
      AudioInfo current = currentInfo();
      if (current != cfg) {
        cfg = current;
        cfg.logInfo();
        if (p_notify!=nullptr){
          p_notify->setAudioInfo(cfg);
        } else {
          LOGW("p_notify is null");
        }
      }
      p_out->write(pcm.data(), result);
      return true;
    } else {
      LOGE("copy: %ld - %s", result, readError(result));
      return false;
    }
  }

protected:
  AudioInfo cfg;
  AudioInfoSupport *p_notify = nullptr;
  Print *p_out = nullptr;
  Stream *p_in = nullptr;
  Vector<uint8_t> pcm;
  OggVorbis_File file;
  ov_callbacks callbacks;
  bool active;
  int bitstream;
  bool is_first = true;

  AudioInfo currentInfo() {
    AudioInfo result;
    vorbis_info *info = ov_info(&file, -1);
    result.sample_rate = info->rate;
    result.channels = info->channels;
    result.bits_per_sample = 2;
    return result;
  }

  virtual size_t readBytes(uint8_t *ptr, size_t size) override {
    size_t read_size =  min(size,(size_t)VARBIS_MAX_READ_SIZE);
    size_t result = p_in->readBytes((uint8_t *)ptr, read_size);
    LOGD("readBytes: %ld",result);
    return result;
  }

  static size_t read_func(void *ptr, size_t size, size_t nmemb,
                          void *datasource) {
    VorbisDecoder *self = (VorbisDecoder *)datasource;
    return self->readBytes((uint8_t *)ptr, size);
  }

  static int seek_func(void *datasource, ogg_int64_t offset, int whence) {
    VorbisDecoder *self = (VorbisDecoder *)datasource;
    return -1;
  }

  static long tell_func(void *datasource) {
    VorbisDecoder *self = (VorbisDecoder *)datasource;
    return -1;
  }

  static int close_func(void *datasource) {
    VorbisDecoder *self = (VorbisDecoder *)datasource;
    self->end();
    return 0;
  }

  const char* readError(long error){
    switch(error){
      case OV_HOLE:
        return "Interruption in the data";
      case OV_EBADLINK:
        return "Invalid stream section ";
      case OV_EINVAL:
        return "Invalid header";
      default:
        return "N/A";
    }
  }

};

} // namespace audio_tools
