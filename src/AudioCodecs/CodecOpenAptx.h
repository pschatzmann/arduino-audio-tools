/**
 * @file CodecAptx.h
 * @author Phil Schatzmann
 * @brief Codec for aptx using https://github.com/pschatzmann/libopenaptx
 * @version 0.1
 * @date 2022-04-24
 *
 * @copyright Copyright (c) 2022
 *
 */
#pragma once
#include "openaptx.h"

namespace audio_tools {

/**
 * @brief Decoder for OpenAptx. Depends on
 * https://github.com/pschatzmann/libopenaptx
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class OpenAptxDecoder : public AudioDecoder {
 public:
  OpenAptxDecoder(bool isHd=false) {
    is_hd = isHd;
    info.sample_rate = 44100;
    info.channels = 2;
    info.bits_per_sample = isHd ? 24 : 16;
  }
  virtual AudioBaseInfo audioInfo() { return info; }

  virtual void begin() {
    ctx = aptx_init(is_hd);
    is_first_write = true;
    if (notify != nullptr) {
      notify->setAudioInfo(info);
    }
  }

  virtual void end() {
    bool dropped = aptx_decode_sync_finish(ctx);
    aptx_finish(ctx);
  }

  virtual void setNotifyAudioChange(AudioBaseInfoDependent &bi) {
    notify = &bi;
  }

  virtual void setOutputStream(Print &out_stream) { p_print = &out_stream; }

  operator boolean() { return ctx != nullptr; }

  virtual size_t write(const void *input_buffer, size_t length) {
    int ret = 0;
    if (is_first_write) {
      is_first_write = false;
      if (!checkPrefix(input_buffer, length)){
        return 0;
      }
    }

    processed =
        aptx_decode_sync(ctx, (const uint8_t*)input_buffer, length, output_buffer,
                         sizeof(output_buffer), &written, &synced, &dropped);

    /* Check all possible states of synced, syncing and dropped status */
    if (!synced) {
      if (!syncing) {
        LOGE("aptX decoding failed, synchronizing");
        syncing = 1;
        ret = 1;
      }
      if (dropped) {
        LOGE("aptX synchronization successful, dropped %lu byte%s",
             (unsigned long)dropped, (dropped != 1) ? "s" : "");
        syncing = 0;
        ret = 1;
      }
      if (!syncing) {
        LOGE("aptX decoding failed, synchronizing");
        syncing = 1;
        ret = 1;
      }
    } else {
      if (dropped) {
        if (!syncing) LOGE("aptX decoding failed, synchronizing");
        LOGE("aptX synchronization successful, dropped %lu byte%s", 
             (unsigned long)dropped, (dropped != 1) ? "s" : "");
        syncing = 0;
        ret = 1;
      } else if (syncing) {
        LOGE("aptX synchronization successful" );
        syncing = 0;
        ret = 1;
      }
    }

    /* If we have not decoded all supplied samples then decoding unrecoverable
     * failed */
    if (processed != length) {
      LOGE("aptX decoding failed");
      ret = 1;
    }

    if (written > 0) {
      if (p_print->write(output_buffer, written) != written) {
        LOGE("aptX decoding failed to write decoded data");
        ret = 1;
      }
    }

    return ret == 1 ? 0 : length;
  }


 protected:
  Print *p_print = nullptr;
  AudioBaseInfo info;
  struct aptx_context *ctx = nullptr;
  AudioBaseInfoDependent *notify = nullptr;
  bool is_first_write = true;
  uint8_t output_buffer[512 * 3 * 2 * 6 + 3 * 2 * 4];
  bool is_hd;
  size_t processed;
  size_t written;
  size_t dropped;
  int synced;
  int syncing;

  bool checkPrefix(const void *input_buffer, size_t length) {
    bool result = true;
    if (length >= 4 && memcmp(input_buffer, "\x4b\xbf\x4b\xbf", 4) == 0) {
      if (is_hd) {
        LOGE("aptX audio stream (not aptX HD)");
        result = false;
      }
    } else if (length >= 6 &&
               memcmp(input_buffer, "\x73\xbe\xff\x73\xbe\xff", 6) == 0) {
      if (!is_hd) {
        LOGE("aptX HD audio stream");
        result = false;
      }
    } else {
      if (length >= 4 && memcmp(input_buffer, "\x6b\xbf\x6b\xbf", 4) == 0) {
        LOGE("standard aptX audio stream - not supported");
        result = false;
      } else {
        LOGE("No aptX nor aptX HD audio stream");
        result = false;
      }
    }
    return result;
  }
};

/**
 * @brief Encoder for OpenAptx - Depends on
 * https://github.com/pschatzmann/libopenaptx
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class OpenAptxEncoder : public AudioEncoder {
 public:
  OpenAptxEncoder(bool isHd = false) {
    is_hd = isHd;
    info.sample_rate = 44100;
    info.channels = 2;
    info.bits_per_sample = isHd ? 24 : 16;
  }

  void begin() { ctx = aptx_init(is_hd); }

  virtual void end() {
    size_t written;
    aptx_encode_finish(ctx, output_buffer, sizeof(output_buffer), &written);
    p_print->write((const uint8_t*)output_buffer, written);
    aptx_finish(ctx);
    ctx = nullptr;
  }

  virtual const char *mime() { return "audio/aptx"; }

  virtual void setAudioInfo(AudioBaseInfo info) { this->info = info; }

  virtual void setOutputStream(Print &out_stream) { p_print = &out_stream; }

  operator boolean() { return ctx != nullptr; }

  virtual size_t write(const void *in_ptr, size_t in_size) {
    size_t written=0;

    size_t result = aptx_encode(ctx, (const uint8_t*) in_ptr, in_size, output_buffer,
                               sizeof(output_buffer), &written);
    p_print->write((const uint8_t*)output_buffer, written);
    return result;
  }

 protected:
  bool is_hd;
  AudioBaseInfo info;
  Print *p_print = nullptr;
  struct aptx_context *ctx = nullptr;
  uint8_t output_buffer[512 * 6];
};

}  // namespace audio_tools