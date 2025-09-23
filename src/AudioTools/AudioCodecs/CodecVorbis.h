#pragma once
#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioToolsConfig.h"
#include "ogg.h"
#include "vorbis-tremor.h"

// #include "AudioTools/AudioCodecs/ContainerOgg.h"
// #include "ivorbiscodec.h"
// #include "ivorbisfile.h"

namespace audio_tools {

#ifndef VARBIS_MAX_READ_SIZE
#define VARBIS_MAX_READ_SIZE 1024
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

    // Ensure we start with clean state
    if (active) {
      LOGW("Decoder already active, calling end() first");
      end();
    }

    callbacks.read_func = read_func;
    callbacks.seek_func = seek_func;
    callbacks.close_func = nullptr;
    callbacks.tell_func = tell_func;

    assert(p_input != nullptr);
    if (p_input->available() < VORBIS_HEADER_OPEN_LIMIT) {
      delay(delay_wait_for_data_ms);
    }
    LOGI("available: %d", p_input->available());

    is_ov_open = ovOpen();
    LOGI("ovOpen result: %d", is_ov_open);

    active = is_ov_open;
    return is_ov_open;
  }

  /// Releases the reserved memory
  void end() override {
    LOGI("end");
    if (is_ov_open && active) {
      ov_clear(&file);
      LOGI("ov_clear completed");
    }
    is_ov_open = false;
    is_first = true;
    active = false;
    pcm.clear();  // Free the PCM buffer
  }

  /// Provides the last available MP3FrameInfo
  AudioInfo audioInfo() override { return cfg; }

  /// checks if the class is active
  virtual operator bool() override { return active; }

  virtual bool copy() override {
    TRACED();

    // open if not already done
    if (!is_ov_open) {
      if (!ovOpen()) {
        LOGE("Failed to open Vorbis stream");
        return false;
      }
    }

    // Defensive checks before calling Vorbis functions
    if (pcm.data() == nullptr) {
      LOGE("PCM buffer is null - memory allocation failed");
      return false;
    }

    if (pcm.size() == 0) {
      LOGE("PCM buffer size is 0");
      return false;
    }

    // Additional sanity check for the file structure
    if (!active) {
      LOGE("Decoder is not active");
      return false;
    }

    LOGD("ov_read: buffer size %d", pcm.size());
    bitstream = 0;

    // Call ov_read with additional error checking
    long result = ov_read(&file, (char *)pcm.data(), pcm.size(), &bitstream);
    LOGI("copy result: %d", (int)result);

    if (result > 0) {
      AudioInfo current = currentInfo();
      if (current != cfg) {
        cfg = current;
        cfg.logInfo();
        notifyAudioChange(cfg);
      }

      if (p_print != nullptr) {
        p_print->write(pcm.data(), result);
      } else {
        LOGE("Output stream is null");
        return false;
      }
      delay(1);
      return true;
    } else {
      if (result == 0 || result == -3) {
        // data interruption
        LOGD("copy: %d - %s", (int)result, readError(result));
      } else {
        LOGE("copy: %d - %s", (int)result, readError(result));
      }
      delay(delay_on_no_data_ms);
      return false;
    }
  }

  /// Provides "audio/ogg"
  const char *mime() override { return "audio/vorbis+ogg"; }

  /// Defines the delay when there is no data
  void setDelayOnNoData(size_t delay) { delay_on_no_data_ms = delay; }

  /// Defines the delay to wait if there is not enough data to open the decoder
  void setWaitForData(size_t wait) { delay_wait_for_data_ms = wait; }

  /// Defines the default read size
  void setReadSize(size_t size) {
    max_read_size = size;
    // Ensure we don't set an unreasonably large size
    if (max_read_size > 8192) {
      LOGW("Read size %zu is very large, consider smaller buffer",
           max_read_size);
    }
  }

 protected:
  AudioInfo cfg;
  Vector<uint8_t> pcm{0};
  OggVorbis_File file;
  ov_callbacks callbacks;
  int bitstream = 0;
  size_t delay_on_no_data_ms = 100;
  size_t delay_wait_for_data_ms = 500;
  size_t max_read_size = VARBIS_MAX_READ_SIZE;
  bool active = false;
  bool is_first = true;
  bool is_ov_open = false;

  bool ovOpen() {
    pcm.resize(max_read_size);
    checkMemory(true);
    int rc = ov_open_callbacks(this, &file, nullptr, 0, callbacks);
    if (rc < 0) {
      LOGE("ov_open_callbacks failed with error %d: %s", rc, getOpenError(rc));
    } else {
      LOGI("ov_open_callbacks succeeded");
      is_ov_open = true;
    }
    checkMemory(true);
    return is_ov_open;
  }

  AudioInfo currentInfo() {
    AudioInfo result;
    if (!is_ov_open) {
      LOGE("Cannot get audio info - stream not open");
      return result;
    }

    vorbis_info *info = ov_info(&file, -1);
    if (info == nullptr) {
      LOGE("ov_info returned null pointer");
      return result;
    }

    result.sample_rate = info->rate;
    result.channels = info->channels;
    result.bits_per_sample = 16;

    LOGD("Audio info - rate: %d, channels: %d", info->rate, info->channels);
    return result;
  }

  virtual size_t readBytes(uint8_t *data, size_t len) override {
    size_t read_size = min(len, (size_t)max_read_size);
    size_t result = p_input->readBytes((uint8_t *)data, read_size);
    LOGD("readBytes: %zu", result);
    return result;
  }

  static size_t read_func(void *ptr, size_t size, size_t nmemb,
                          void *datasource) {
    VorbisDecoder *self = (VorbisDecoder *)datasource;
    assert(datasource != nullptr);
    size_t result = self->readBytes((uint8_t *)ptr, size * nmemb);
    LOGD("read_func: %d -> %d", size * nmemb, (int)result);
    return result;
  }

  static int seek_func(void *datasource, ogg_int64_t offset, int whence) {
    VorbisDecoder *self = (VorbisDecoder *)datasource;
    return -1;
  }

  static long tell_func(void *datasource) {
    VorbisDecoder *self = (VorbisDecoder *)datasource;
    return -1;
  }

  // static int close_func(void *datasource) {
  //   VorbisDecoder *self = (VorbisDecoder *)datasource;
  //   self->end();
  //   return 0;
  // }

  const char *readError(long error) {
    if (error >= 0) {
      return "OK";
    }
    switch (error) {
      case OV_HOLE:
        return "Interruption in the data";
      case OV_EBADLINK:
        return "Invalid stream section";
      case OV_EREAD:
        return "Read error";
      case OV_EFAULT:
        return "Internal fault";
      case OV_EIMPL:
        return "Unimplemented feature";
      case OV_EINVAL:
        return "Invalid argument";
      case OV_ENOTVORBIS:
        return "Not a Vorbis file";
      case OV_EBADHEADER:
        return "Invalid Vorbis header";
      case OV_EVERSION:
        return "Vorbis version mismatch";
      case OV_ENOSEEK:
        return "Stream not seekable";
      default:
        return "Unknown error";
    }
  }

  const char *getOpenError(int error) {
    switch (error) {
      case 0:
        return "Success";
      case OV_EREAD:
        return "Read from media error";
      case OV_ENOTVORBIS:
        return "Not Vorbis data";
      case OV_EVERSION:
        return "Vorbis version mismatch";
      case OV_EBADHEADER:
        return "Invalid Vorbis bitstream header";
      case OV_EFAULT:
        return "Internal logic fault";
      default:
        return "Unknown open error";
    }
  }
};

}  // namespace audio_tools
