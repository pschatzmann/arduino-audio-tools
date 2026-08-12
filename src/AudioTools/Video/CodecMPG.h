#pragma once

#include <memory>
#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/Video/CodecVideo.h"

#if __has_include("TinyMPGDecoder.h")
#include "TinyMPGDecoder.h"
#define AUDIO_TOOLS_HAS_TINY_MPG_DECODER
#endif

#if __has_include("TinyMPGEncoder.h")
#include "TinyMPGEncoder.h"
#define AUDIO_TOOLS_HAS_TINY_MPG_ENCODER
#endif

#if !defined(AUDIO_TOOLS_HAS_TINY_MPG_DECODER)
#error "CodecMPG.h requires TinyMPGDecoder.h. Install TinyMPG: https://github.com/pschatzmann/TinyMPG"
#endif

#if !defined(AUDIO_TOOLS_HAS_TINY_MPG_ENCODER)
#error "CodecMPG.h requires TinyMPGEncoder.h. Install TinyMPG: https://github.com/pschatzmann/TinyMPG"
#endif

#ifndef MPG_DECODER_DEFAULT_ALLOCATOR
#ifdef ESP32
#define MPG_DECODER_DEFAULT_ALLOCATOR tinympg::PSRAMAllocatorESP32<uint8_t>
#else
#define MPG_DECODER_DEFAULT_ALLOCATOR std::allocator<uint8_t>
#endif
#endif

#ifndef MPG_ENCODER_DEFAULT_ALLOCATOR
#ifdef ESP32
#define MPG_ENCODER_DEFAULT_ALLOCATOR tinympg::PSRAMAllocatorESP32<uint8_t>
#else
#define MPG_ENCODER_DEFAULT_ALLOCATOR std::allocator<uint8_t>
#endif
#endif

/**
 * @defgroup mpg MPG
 * @ingroup codecs
 * @ingroup video
 * @brief MPEG-1 part 2 (video) encoding/decoding using https://github.com/pschatzmann/TinyMPG
 *
 * Dependency note:
 * - TinyMPG must be installed and visible in your include path.
 * - Arduino IDE/CLI: install TinyMPG as an Arduino library so
 *   `TinyMPGDecoder.h` and `TinyMPGEncoder.h` can be resolved.
 * - Project link: https://github.com/pschatzmann/TinyMPG
 */

namespace audio_tools {

/**
 * @brief MPEG-1 (part 2) video decoder wrapper around TinyMPGDecoder.
 *
 * The class mirrors the usage style of CodecH264.h: feed MPEG-1 bitstream
 * data via write(), and receive decoded frames on the configured output.
 *
 * Note: Requires TinyMPG (`TinyMPGDecoder.h`) to be installed and reachable
 * by the compiler include path:
 * https://github.com/pschatzmann/TinyMPG
 *
 * @ingroup mpg
 * @ingroup decoder
 */
template <typename Allocator = MPG_DECODER_DEFAULT_ALLOCATOR>
class MPGDecoder : public VideoDecoder {
 public:
  MPGDecoder() { decoder_.setCallback(onFrame, this); }

  void setOutput(Print &out) override { p_out = &out; }

  void setVideoFormat(VideoFormat format) override {
    switch (format) {
      case VideoFormat::RGB565:
      case VideoFormat::RGB888:
      case VideoFormat::I420:
        pixel_format = format;
        break;
      default:
        LOGW("MPGDecoder: unsupported VideoFormat %d", (int)format);
        break;
    }
  }

  VideoInfo videoInfo() override {
    VideoInfo info;
    info.format = pixel_format;
    info.width = (uint16_t)decoder_.width();
    info.height = (uint16_t)decoder_.height();
    return info;
  }

  bool begin() override {
    decoder_.begin();
    return true;
  }

  void end() override { decoder_.end(); }

  size_t write(const uint8_t *data, size_t len) override {
    decoder_.write(data, len);
    return len;
  }

  void flush() override {}

  bool hasError() { return decoder_.hasError(); }

  tinympg::TinyMPGDecoder<Allocator> &driver() { return decoder_; }

 protected:
  tinympg::TinyMPGDecoder<Allocator> decoder_;
  Print *p_out = nullptr;
  VideoFormat pixel_format = VideoFormat::RGB565;
  Vector<uint8_t> frame_buffer;

  static void onFrame(tinympg::TinyMPGDecoder<Allocator> &decoder,
                      void *userData) {
    static_cast<MPGDecoder *>(userData)->writeFrame(decoder);
  }

  void writeFrame(tinympg::TinyMPGDecoder<Allocator> &decoder) {
    if (p_out == nullptr) return;

    size_t w = decoder.width();
    size_t h = decoder.height();
    size_t n = 0;

    switch (pixel_format) {
      case VideoFormat::RGB565: {
        size_t needed = w * h;
        if (frame_buffer.size() < needed * 2) frame_buffer.resize(needed * 2);
        n = decoder.toRGB565((uint16_t *)frame_buffer.data(), needed);
        if (n > 0) p_out->write(frame_buffer.data(), n * 2);
        break;
      }
      case VideoFormat::RGB888: {
        size_t needed = w * h * 3;
        if (frame_buffer.size() < needed) frame_buffer.resize(needed);
        n = decoder.toRGB888(frame_buffer.data(), needed);
        if (n > 0) p_out->write(frame_buffer.data(), n);
        break;
      }
      case VideoFormat::I420: {
        size_t needed = w * h + 2 * (w / 2) * (h / 2);
        if (frame_buffer.size() < needed) frame_buffer.resize(needed);
        n = decoder.toYUV420(frame_buffer.data(), needed);
        if (n > 0) p_out->write(frame_buffer.data(), n);
        break;
      }
      default:
        break;
    }
  }
};

/**
 * @brief MPEG-1 part 2 video encoder wrapper around TinyMPGEncoder.
 *
 * This mirrors the API shape of H264Encoder in CodecH264.h.
 *
 * Note: Requires TinyMPG (`TinyMPGEncoder.h`) to be installed and reachable
 * by the compiler include path:
 * https://github.com/pschatzmann/TinyMPG
 *
 * @ingroup mpg
 * @ingroup encoder
 */
template <typename Allocator = MPG_ENCODER_DEFAULT_ALLOCATOR>
class MPGEncoder : public VideoEncoder {
 public:
  MPGEncoder(int width = 0, int height = 0) : encoder_(width, height) {
    width_ = width;
    height_ = height;
  }

  void setOutput(Print &out) override { p_out = &out; }

  void setVideoFormat(VideoFormat format) override {
    switch (format) {
      case VideoFormat::I420:
      case VideoFormat::YUV422:
      case VideoFormat::RGB565:
      case VideoFormat::RGB888:
        input_format = format;
        break;
      default:
        LOGW("MPGEncoder: unsupported VideoFormat %d", (int)format);
        break;
    }
  }

  VideoInfo videoInfo() override {
    VideoInfo info;
    info.format = VideoFormat::MPEG1;
    info.width = (uint16_t)width_;
    info.height = (uint16_t)height_;
    return info;
  }

  void setSize(int width, int height) {
    width_ = width;
    height_ = height;
    encoder_.setSize(width, height);
  }

  bool begin() override { return encoder_.begin(); }

  void end() override { encoder_.end(); }

  size_t write(const uint8_t *data, size_t len) override {
    if (p_out == nullptr) return 0;
    return encoder_.encode(data, len, *p_out, input_format);
  }

  tinympg::TinyMPGEncoder<Allocator> &driver() { return encoder_; }

 protected:
  tinympg::TinyMPGEncoder<Allocator> encoder_;
  Print *p_out = nullptr;
  VideoFormat input_format = VideoFormat::I420;
  int width_ = 0;
  int height_ = 0;
};

}  // namespace audio_tools
