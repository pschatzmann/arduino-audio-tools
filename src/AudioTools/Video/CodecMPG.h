#pragma once

#include <memory>
#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/Video/CodecVideo.h"

#include "TinyMPGDecoder.h"
#include "TinyMPGEncoder.h"


#ifndef MPG_DEFAULT_ALLOCATOR
#ifdef ESP32
#  define MPG_DEFAULT_ALLOCATOR tinympg::PSRAMAllocatorESP32<uint8_t>
#else
#  define MPG_DEFAULT_ALLOCATOR tinympg::StdAllocator<uint8_t>
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
class MPGDecoder : public VideoDecoder {
 public:
  MPGDecoder() { decoder_.setCallback(onFrame, this); }

  void setOutput(Print &out) override { p_out = &out; }

  void setVideoFormat(VideoFormat format) override {
    switch (format) {
      case VideoFormat::RGB565:
      case VideoFormat::RGB666:
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

  tinympg::TinyMPGDecoder<MPG_DEFAULT_ALLOCATOR> &driver() { return decoder_; }

 protected:
  tinympg::TinyMPGDecoder<MPG_DEFAULT_ALLOCATOR> decoder_;
  Print *p_out = nullptr;
  VideoFormat pixel_format = VideoFormat::RGB565;
  Vector<uint8_t> frame_buffer;

  static void onFrame(tinympg::TinyMPGDecoder<MPG_DEFAULT_ALLOCATOR> &decoder,
                      void *userData) {
    static_cast<MPGDecoder *>(userData)->writeFrame(decoder);
  }

  void writeFrame(tinympg::TinyMPGDecoder<MPG_DEFAULT_ALLOCATOR> &decoder) {
    if (p_out == nullptr) return;

    size_t w = decoder.width();
    size_t h = decoder.height();
    size_t n = 0;

    switch (pixel_format) {
      case VideoFormat::RGB565: {
        size_t needed = w * h;
        if (frame_buffer.size() < needed * 2) {
          frame_buffer.resize(needed * 2);
          if (frame_buffer.data() == nullptr) {
            LOGE("MPGDecoder: frame buffer allocation failed (RGB565)");
            return;
          }
        }
        n = decoder.toRGB565((uint16_t *)frame_buffer.data(), needed);
        if (n > 0) p_out->write(frame_buffer.data(), n * 2);
        break;
      }
      case VideoFormat::RGB888: {
        size_t needed = w * h * 3;
        if (frame_buffer.size() < needed) {
          frame_buffer.resize(needed);
          if (frame_buffer.data() == nullptr) {
            LOGE("MPGDecoder: frame buffer allocation failed (RGB888)");
            return;
          }
        }
        n = decoder.toRGB888(frame_buffer.data(), needed);
        if (n > 0) p_out->write(frame_buffer.data(), n);
        break;
      }
      case VideoFormat::RGB666: {
        size_t needed = w * h * 3;
        if (frame_buffer.size() < needed) {
          frame_buffer.resize(needed);
          if (frame_buffer.data() == nullptr) {
            LOGE("MPGDecoder: frame buffer allocation failed (RGB666)");
            return;
          }
        }
        n = decoder.toRGB666(frame_buffer.data(), needed);
        if (n > 0) p_out->write(frame_buffer.data(), n);
        break;
      }
      case VideoFormat::I420: {
        size_t needed = w * h + 2 * (w / 2) * (h / 2);
        if (frame_buffer.size() < needed) {
          frame_buffer.resize(needed);
          if (frame_buffer.data() == nullptr) {
            LOGE("MPGDecoder: frame buffer allocation failed (I420)");
            return;
          }
        }
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
class MPGEncoder : public VideoEncoder {
 public:
  MPGEncoder(int width = 0, int height = 0) {
    width_ = width;
    height_ = height;
    encoder_.setQp(8);
    if (width_ > 0 && height_ > 0) {
      encoder_.setSize(width_, height_);
      ensureBitstreamBuffer();
    }
  }

  void setOutput(Print &out) override { p_out = &out; }

  void setVideoFormat(VideoFormat format) override {
    switch (format) {
      case VideoFormat::I420:
      case VideoFormat::YUV422:
      case VideoFormat::RGB565:
      case VideoFormat::RGB666:
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
    ensureBitstreamBuffer();
  }

  void setQp(int qscale) { encoder_.setQp(qscale); }

  void setFrameRate(int fps) { encoder_.setFrameRate(fps); }

  void setBitstreamBufferSize(size_t bytes) {
    bitstream_buffer.resize(bytes);
    if (bitstream_buffer.data() == nullptr && bytes > 0) {
      LOGE("MPGEncoder: bitstream buffer allocation failed");
    }
  }

  bool begin() override {
    if (width_ <= 0 || height_ <= 0) {
      LOGE("MPGEncoder: invalid size - call setSize() first");
      return false;
    }
    if (!ensureBitstreamBuffer()){
      LOGE("MPGEncoder: failed to allocate bitstream buffer");
      return false;
    }
    return true;
  }

  void end() override {}

  size_t write(const uint8_t *data, size_t len) override {
    if (p_out == nullptr) return 0;
    if (width_ <= 0 || height_ <= 0) {
      LOGE("MPGEncoder: invalid size - call setSize() first");
      return 0;
    }
    if (!ensureBitstreamBuffer()) {
      LOGE("MPGEncoder: bitstream buffer not available");
      return 0;
    }

    switch (input_format) {
      case VideoFormat::I420: {
        size_t y_size = (size_t)width_ * (size_t)height_;
        size_t uv_size = y_size / 4;
        size_t min_size = y_size + uv_size + uv_size;
        if (len < min_size) {
          LOGE("MPGEncoder: input buffer too small for I420 frame");
          return 0;
        }
        const uint8_t *srcY = data;
        const uint8_t *srcU = data + y_size;
        const uint8_t *srcV = data + y_size + uv_size;
        return writeOut([&](uint8_t *dst, size_t cap) {
          return encoder_.encodeFrame(srcY, srcU, srcV, dst, cap);
        });
      }
      case VideoFormat::YUV422:
        return writeOut([&](uint8_t *dst, size_t cap) {
          return encoder_.encodeFrameYuv422(data, dst, cap);
        });
      case VideoFormat::RGB565:
        return writeOut([&](uint8_t *dst, size_t cap) {
          return encoder_.encodeFrameRgb565((const uint16_t *)data, dst, cap);
        });
      case VideoFormat::RGB666:
        return writeOut([&](uint8_t *dst, size_t cap) {
          return encoder_.encodeFrameRgb666(data, dst, cap);
        });
      case VideoFormat::RGB888:
        return writeOut([&](uint8_t *dst, size_t cap) {
          return encoder_.encodeFrameRgb888(data, dst, cap);
        });
      default:
        LOGE("MPGEncoder: unsupported VideoFormat %d", (int)input_format);
        return 0;
    }
  }

  tinympg::TinyMPGEncoder<MPG_DEFAULT_ALLOCATOR> &driver() { return encoder_; }

 protected:
  static constexpr int kMaxGrowRetries = 4;

  tinympg::TinyMPGEncoder<MPG_DEFAULT_ALLOCATOR> encoder_;
  Print *p_out = nullptr;
  VideoFormat input_format = VideoFormat::I420;
  int width_ = 0;
  int height_ = 0;
  Vector<uint8_t> bitstream_buffer;

  template <typename Fn>
  size_t writeOut(Fn encodeInto) {
    if (p_out == nullptr) return 0;
    size_t encoded = encodeInto(bitstream_buffer.data(), bitstream_buffer.size());
    for (int attempt = 0; encoded == 0 && attempt < kMaxGrowRetries; ++attempt) {
      bitstream_buffer.resize(bitstream_buffer.size() * 2);
      if (bitstream_buffer.data() == nullptr) {
        LOGE("MPGEncoder: bitstream buffer grow failed");
        return 0;
      }
      encoded = encodeInto(bitstream_buffer.data(), bitstream_buffer.size());
    }
    if (encoded == 0) {
      LOGE("MPGEncoder: encode failed (status=%d)", (int)encoder_.lastStatus());
      return 0;
    }
    return p_out->write(bitstream_buffer.data(), encoded);
  }

  bool ensureBitstreamBuffer() {
    if (width_ <= 0 || height_ <= 0) return false;
    size_t needed = (size_t)width_ * (size_t)height_ * 2 + 4096;
    if (bitstream_buffer.size() < needed) {
      bitstream_buffer.resize(needed);
      if (bitstream_buffer.data() == nullptr) {
        return false;
      }
    }
    return bitstream_buffer.data() != nullptr;
  }
};

}  // namespace audio_tools
