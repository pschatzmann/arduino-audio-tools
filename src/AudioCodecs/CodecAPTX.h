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
#include "AudioConfig.h"
#include "AudioCodecs/AudioCodecsBase.h"
#include "openaptx.h"

namespace audio_tools {

/**
 * @brief Decoder for OpenAptx. Depends on
 * https://github.com/pschatzmann/libopenaptx
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class APTXDecoder : public AudioDecoder {
 public:
  APTXDecoder(bool isHd = false) {
    is_hd = isHd;
    info.sample_rate = 44100;
    info.channels = 2;
    info.bits_per_sample = isHd ? 24 : 16;
  }

  bool begin() override {
    TRACEI();
    ctx = aptx_init(is_hd);
    is_first_write = true;
    notifyAudioChange(info);
    return ctx != nullptr;
  }

  void end() override {
    TRACEI();
    bool dropped = aptx_decode_sync_finish(ctx);
    aptx_finish(ctx);
    ctx = nullptr;
  }

  virtual void setOutput(Print &out_stream) { p_print = &out_stream; }

  operator bool() { return ctx != nullptr; }

  virtual size_t write(const uint8_t *data, size_t len) {
    LOGI("write: %d", len);
    bool is_ok = true;
    size_t dropped;
    int synced;

    if (is_first_write) {
      is_first_write = false;
      if (!checkPrefix(data, len)) {
        return 0;
      }
    }

    output_buffer.resize(len * 10);
    memset(output_buffer.data(), 0, output_buffer.size());
    processed = aptx_decode_sync(ctx, (const uint8_t *)data, len,
                                 output_buffer.data(), output_buffer.size(),
                                 &written, &synced, &dropped);

    checkSync(synced, dropped, is_ok);

    // If we have not decoded all supplied samples then decoding unrecoverable
    // failed
    if (processed != len) {
      LOGE("aptX decoding reqested: %d eff: %d", len, processed);
      is_ok = false;
    }

    writeData(written, is_ok);

    return is_ok ? len : 0;
  }

 protected:
  struct aptx_context *ctx = nullptr;
  Print *p_print = nullptr;
  bool is_first_write = true;
  Vector<uint8_t> output_buffer;
  bool is_hd;
  size_t processed;
  size_t written;
  bool syncing;

  /// Converts the data to 16 bit and writes it to final output
  void writeData(size_t written, bool &is_ok) {
    if (written > 0) {
      int samples = written / 3;
      LOGI("written: %d", written);
      LOGI("samples: %d", samples);
      int24_t *p_int24 = (int24_t *)output_buffer.data();
      int16_t *p_int16 = (int16_t *)output_buffer.data();
      for (int j = 0; j < samples; j++) {
        p_int16[j] = p_int24[j].getAndScale16();
      }

      if (p_print->write((uint8_t *)output_buffer.data(), samples * 2) !=
          samples * 2) {
        LOGE("aptX decoding failed to write decoded data");
        is_ok = false;
      }
    }
  }

  /// Checks the syncronization
  void checkSync(bool synced, bool dropped, bool &is_ok) {
    /* Check all possible states of synced, syncing and dropped status */
    if (!synced) {
      if (!syncing) {
        LOGE("aptX decoding failed, synchronizing");
        syncing = true;
        is_ok = false;
      }
      if (dropped) {
        LOGE("aptX synchronization successful, dropped %lu byte%s",
             (unsigned long)dropped, (dropped != 1) ? "s" : "");
        syncing = false;
        is_ok = true;
      }
      if (!syncing) {
        LOGE("aptX decoding failed, synchronizing");
        syncing = true;
        is_ok = false;
      }
    } else {
      if (dropped) {
        if (!syncing) LOGE("aptX decoding failed, synchronizing");
        LOGE("aptX synchronization successful, dropped %lu byte%s",
             (unsigned long)dropped, (dropped != 1) ? "s" : "");
        syncing = false;
        is_ok = false;
      } else if (syncing) {
        LOGI("aptX synchronization successful");
        syncing = false;
        is_ok = true;
      }
    }
  }

  /// Checks the prefix of the received data
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
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class APTXEncoder : public AudioEncoder {
 public:
  APTXEncoder(bool isHd = false) {
    is_hd = isHd;
    info.sample_rate = 44100;
    info.channels = 2;
    info.bits_per_sample = isHd ? 24 : 16;
  }

  bool begin() {
    TRACEI();
    input_buffer.resize(4 * 2);
    output_buffer.resize(100 * (is_hd ? 6 : 4));

    LOGI("input_buffer.size: %d", input_buffer.size());
    LOGI("output_buffer.size: %d", output_buffer.size());
    LOGI("is_hd: %s", is_hd ? "true" : "false");
    ctx = aptx_init(is_hd);
    return ctx!=nullptr;
  }

  virtual void end() {
    TRACEI();
    if (ctx != nullptr) {
      size_t output_written = 0;
      aptx_encode_finish(ctx, output_buffer.data(), output_buffer.size(),
                         &output_written);
      if (output_written > 0) {
        // write result to final output
        int written = p_print->write((const uint8_t *)output_buffer.data(),
                                     output_written);
        if (written != output_written) {
          LOGE("write requested: %d eff: %d", output_written, written);
        }
      }
      aptx_finish(ctx);
      ctx = nullptr;
    }
  }

  virtual const char *mime() { return "audio/aptx"; }

  virtual void setAudioInfo(AudioInfo info) {
    AudioEncoder::setAudioInfo(info);
    switch (info.bits_per_sample) {
      case 16:
        is_hd = false;
        break;
      case 24:
        is_hd = true;
        break;
      default:
        LOGE("invalid bits_per_sample: %d", info.bits_per_sample);
    }
  }

  virtual void setOutput(Print &out_stream) { p_print = &out_stream; }

  operator bool() { return ctx != nullptr; }

  virtual size_t write(const uint8_t *data, size_t len) {
    LOGI("write: %d", len);
    if (ctx == nullptr) return 0;
    size_t output_written = 0;

    // process all bytes
    int16_t *in_ptr16 = (int16_t *)data;
    int in_samples = len / 2;
    for (int j = 0; j < in_samples; j++) {
      input_buffer[input_pos++].setAndScale16(in_ptr16[j]);

      // if input_buffer is full we encode
      if (input_pos >= input_buffer.size()) {
        size_t result = aptx_encode(
            ctx, (const uint8_t *)input_buffer.data(), input_buffer.size() * 3,
            output_buffer.data() + output_pos,
            output_buffer.size() - output_pos, &output_written);

        output_pos += output_written;

        if (result != input_buffer.size() * 3) {
          LOGW("encode requested: %d, eff: %d", input_buffer.size() * 3,
               result);
        }

        // if output buffer is full we write the result
        if (output_pos + output_pos >= output_buffer.size()) {
          int written =
              p_print->write((const uint8_t *)output_buffer.data(), output_pos);
          if (written != output_pos) {
            LOGE("write requested: %d eff: %d", output_pos, written);
          }
          // restart at beginning of output buffer
          output_pos = 0;
        }
        // restart at beginning of input buffer
        input_pos = 0;
      }
    }

    return len;
  }

 protected:
  bool is_hd;
  Vector<int24_t> input_buffer{4 * 2};
  Vector<uint8_t> output_buffer;
  int input_pos = 0;
  int output_pos = 0;
  Print *p_print = nullptr;
  struct aptx_context *ctx = nullptr;
};

}  // namespace audio_tools