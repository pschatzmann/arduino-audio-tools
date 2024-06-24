#pragma once
#include "AudioConfig.h"
#include "AudioCodecs/AudioCodecsBase.h"
#include "vorbis-tremor.h"

// #include "AudioCodecs/ContainerOgg.h"
// #include "ivorbiscodec.h"
// #include "ivorbisfile.h"


namespace audio_tools {

#ifndef VARBIS_MAX_READ_SIZE
#  define VARBIS_MAX_READ_SIZE 256
#endif

#define VORBIS_HEADER_OPEN_LIMIT 1024

/**
 * @brief Vorbis Streaming Decoder using
 * https://github.com/pschatzmann/arduino-libvorbis-tremor
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

  /// Starts the processing
  bool begin() override {
    LOGI("begin");

    callbacks.read_func = read_func;
    callbacks.seek_func = seek_func;
    callbacks.close_func = close_func;
    callbacks.tell_func = tell_func;

    if (p_input->available()>=VORBIS_HEADER_OPEN_LIMIT){
      ovOpen();
    }

    active = true;
    is_first = true;
    return true;
  }

  /// Releases the reserved memory
  void end() override {
    LOGI("end");
    active = false;
    is_ov_open = false;
    is_first = true;
    ov_clear(&file);
  }

  /// Provides the last available MP3FrameInfo
  AudioInfo audioInfo() override { return cfg; }

  /// checks if the class is active
  virtual operator bool() override { return active; }

  virtual bool copy() override {
    // wait for data
    if (is_first){
      // wait for some data
      if(p_input->available()<VORBIS_HEADER_OPEN_LIMIT){
        delay(20);
        return false;
      }
      LOGI("available: %d", p_input->available());
      is_first = false;
    }

    // open if not already done
    if (!is_ov_open){
      if (!ovOpen()) {
        LOGE("not open");
        return false;
      }
    }

    if(pcm.data()==nullptr){
      LOGE("Not enough memory");
      return false;
    }

    // convert to pcm
    long result = ov_read(&file, (char *)pcm.data(), pcm.size(), &bitstream);
    LOGI("copy: %ld", result);
    if (result > 0) {
      AudioInfo current = currentInfo();
      if (current != cfg) {
        cfg = current;
        cfg.logInfo();
        notifyAudioChange(cfg);
      }
      p_print->write(pcm.data(), result);
      delay(1);
      return true;
    } else {
      if (result==-3){
        // data interruption
        LOGD("copy: %ld - %s", result, readError(result));
      } else {
        LOGE("copy: %ld - %s", result, readError(result));
      }
    
      return false;
    }
  }

protected:
  AudioInfo cfg;
  Vector<uint8_t> pcm;
  OggVorbis_File file;
  ov_callbacks callbacks;
  bool active;
  int bitstream;
  bool is_first = true;
  bool is_ov_open = false;

  bool ovOpen(){
    pcm.resize(VARBIS_MAX_READ_SIZE);
    int rc = ov_open_callbacks(this, &file, nullptr, 0, callbacks);
    if (rc<0){
      LOGE("ov_open_callbacks: %d", rc);
    } else {
      is_ov_open = true;
    }
    return is_ov_open;
  }

  AudioInfo currentInfo() {
    AudioInfo result;
    vorbis_info *info = ov_info(&file, -1);
    result.sample_rate = info->rate;
    result.channels = info->channels;
    result.bits_per_sample = 16;
    return result;
  }

  virtual size_t readBytes(uint8_t *data, size_t len) override {
    size_t read_size =  min(len,(size_t)VARBIS_MAX_READ_SIZE);
    size_t result = p_input->readBytes((uint8_t *)data, read_size);
    LOGD("readBytes: %zu",result);
    return result;
  }

  static size_t read_func(void *ptr, size_t size, size_t nmemb,
                          void *datasource) {
    VorbisDecoder *self = (VorbisDecoder *)datasource;
    return self->readBytes((uint8_t *)ptr, size * nmemb);
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
