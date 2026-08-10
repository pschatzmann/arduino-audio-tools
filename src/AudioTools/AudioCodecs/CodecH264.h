#pragma once

#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/Video/Video.h"
#include "TinyH264Decoder.h"
#include "TinyH264Encoder.h"

/**
 * @defgroup h264 H264
 * @ingroup codecs
 * @ingroup video
 * @brief H.264 encoding/decoding using https://github.com/pschatzmann/TinyH264
 */

namespace audio_tools {

/// @brief Pixel format H264Decoder writes decoded pictures to setOutput()
/// in - see TinyH264Decoder's toRGB565()/toRGB666()/toRGB888()/toYUV420()
/// for exactly what each one produces.
/// @ingroup h264
enum class H264PixelFormat { RGB565, RGB666, RGB888, YUV420 };

/**
 * @brief H.264 video decoder: wraps TinyH264Decoder
 * (https://github.com/pschatzmann/TinyH264) as a VideoOutput, so it can be
 * plugged directly into e.g. DemuxerAVI::setOutputVideo()/
 * DemuxerMP4::setOutputVideo(). Consumes an Annex-B H.264 bitstream via
 * write() and, once a complete picture has been decoded, writes it -
 * converted to setPixelFormat()'s format (RGB565 by default, the common TFT
 * wire format) - to the Print target configured via setOutput(), one
 * write() call per decoded picture.
 *
 * write()/flush() implement the VideoOutput contract (see Video.h): write()
 * may be called one or more times per frame - bytes are forwarded to the
 * decoder immediately (Annex-B start codes are self-delimiting, so no
 * internal buffering is needed here; a picture may complete partway through
 * any write() call, not just at flush()). flush() is a no-op, kept only
 * because some producers (e.g. DemuxerAVI/DemuxerMP4) call it
 * unconditionally after each frame.
 *
 * The Allocator template parameter is forwarded to TinyH264Decoder - pass a
 * PSRAM allocator (e.g. PSRAMAllocatorESP32<uint8_t>, from
 * TinyH264/src/PSRAMAllocatorESP32.h) to place the decoded picture buffers
 * there instead of the default heap; see TinyH264Decoder.h's own file
 * comment.
 *
 * @ingroup h264
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <typename Allocator = std::allocator<uint8_t>>
class H264Decoder : public VideoOutput {
 public:
  H264Decoder() { decoder_.setCallback(onFrame, this); }

  /// Defines the target the decoded picture is written to, one write()
  /// call per decoded picture, in the format selected via
  /// setPixelFormat() (RGB565 by default).
  void setOutput(Print &out) { p_out = &out; }

  /// Selects the pixel format written to setOutput()'s target - RGB565
  /// (the default) is what most TFT display libraries expect; RGB666/
  /// RGB888 for higher color depth displays; YUV420 to pass the decoded
  /// planes through unconverted (tightly packed I420: Y bytes, then U
  /// bytes, then V bytes, no row padding - see
  /// TinyH264Decoder::toYUV420()).
  void setPixelFormat(H264PixelFormat format) { pixel_format = format; }

  /// Runtime-active maximum stored reference pictures - see
  /// TinyH264Decoder::setMaxRefFrames().
  void setMaxRefFrames(int n) { decoder_.setMaxRefFrames(n); }

  /// Reserves the decoder's picture buffers up front - see
  /// TinyH264Decoder::begin().
  bool begin() {
    decoder_.begin();
    return true;
  }

  /// Releases the decoder's picture buffers - see TinyH264Decoder::end().
  void end() { decoder_.end(); }

  /// Feeds one chunk of Annex-B H.264 data - may be called more than once
  /// per frame (e.g. from a demuxer that hands over payload in pieces).
  /// Decodes immediately, invoking setOutput()'s Print once per completed
  /// picture from within this call (not deferred to flush()).
  size_t write(const uint8_t *data, size_t len) override {
    decoder_.write(data, len);
    return len;
  }

  /// No-op - kept to satisfy the VideoOutput/Print contract; decoding
  /// already happens synchronously in write() (see class comment).
  void flush() override {}

  /// True if the most recent write() call ended in a bitstream error or
  /// hit an unsupported stream feature.
  bool hasError() { return decoder_.hasError(); }

  /// Direct access to the wrapped TinyH264Decoder, e.g. for its width()/
  /// height()/y()/u()/v()/getY()/getU()/getV() accessors.
  tinyh264::TinyH264Decoder<Allocator> &driver() { return decoder_; }

 protected:
  tinyh264::TinyH264Decoder<Allocator> decoder_;
  Print *p_out = nullptr;
  H264PixelFormat pixel_format = H264PixelFormat::RGB565;
  Vector<uint8_t> frame_buffer;

  static void onFrame(tinyh264::TinyH264Decoder<Allocator> &decoder,
                       void *userData) {
    static_cast<H264Decoder *>(userData)->writeFrame();
  }

  void writeFrame() {
    if (p_out == nullptr) return;
    size_t w = decoder_.width();
    size_t h = decoder_.height();
    size_t n = 0;
    switch (pixel_format) {
      case H264PixelFormat::RGB565: {
        size_t needed = w * h;
        if (frame_buffer.size() < needed * 2) frame_buffer.resize(needed * 2);
        n = decoder_.toRGB565((uint16_t *)frame_buffer.data(), needed);
        if (n > 0) p_out->write(frame_buffer.data(), n * 2);
        break;
      }
      case H264PixelFormat::RGB666: {
        size_t needed = w * h * 3;
        if (frame_buffer.size() < needed) frame_buffer.resize(needed);
        n = decoder_.toRGB666(frame_buffer.data(), needed);
        if (n > 0) p_out->write(frame_buffer.data(), n);
        break;
      }
      case H264PixelFormat::RGB888: {
        size_t needed = w * h * 3;
        if (frame_buffer.size() < needed) frame_buffer.resize(needed);
        n = decoder_.toRGB888(frame_buffer.data(), needed);
        if (n > 0) p_out->write(frame_buffer.data(), n);
        break;
      }
      case H264PixelFormat::YUV420: {
        size_t needed = w * h + 2 * (w / 2) * (h / 2);
        if (frame_buffer.size() < needed) frame_buffer.resize(needed);
        n = decoder_.toYUV420(frame_buffer.data(), needed);
        if (n > 0) p_out->write(frame_buffer.data(), n);
        break;
      }
    }
  }
};

/**
 * @brief H.264 video encoder: wraps TinyH264Encoder
 * (https://github.com/pschatzmann/TinyH264). Configure via setSize()/
 * setQp() (or setTargetBitrate() for rate control) same as TinyH264Encoder,
 * then feed raw pictures via encodeFrame()/encodeFrameRgb888()/
 * encodeFrameRgb666()/encodeFrameRgb565()/encodeFrameYuv422() - the
 * resulting Annex-B bitstream is written to the Print target configured via
 * setOutput() (e.g. a Muxer's addVideoFrame() by way of an adapter, or any
 * other Print) instead of a caller-supplied buffer.
 *
 * The internal bitstream scratch buffer starts at
 * kDefaultBitstreamBufferSize and grows (doubling, up to a few attempts)
 * whenever an encode call comes back short - the same "call again with a
 * bigger buffer" recovery TinyH264Encoder's own encodeFrame() family
 * documents, done automatically so callers don't need to size it by hand.
 * Use setBitstreamBufferSize() to pre-size it (e.g. to avoid the first
 * frame paying for a grow-and-retry).
 *
 * The Allocator template parameter is forwarded to TinyH264Encoder - pass a
 * PSRAM allocator to place the encoder's internal reconstructed-picture
 * buffers there instead of the default heap; see TinyH264Encoder.h's own
 * file comment.
 *
 * @ingroup h264
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <typename Allocator = std::allocator<uint8_t>>
class H264Encoder {
 public:
  /// Default size (bytes) of the internal bitstream scratch buffer -
  /// see setBitstreamBufferSize().
  static constexpr size_t kDefaultBitstreamBufferSize = 16384;

  H264Encoder(int width = 0, int height = 0, int keyframeInterval = 0)
      : encoder_(width, height, keyframeInterval) {}

  /// Defines the target the encoded bitstream is written to, one write()
  /// call per encodeFrame()-family call that actually produced output.
  void setOutput(Print &out) { p_out = &out; }

  /// See TinyH264Encoder::setSize()
  void setSize(int width, int height) { encoder_.setSize(width, height); }
  /// See TinyH264Encoder::setQp()
  void setQp(int qp) { encoder_.setQp(qp); }
  /// See TinyH264Encoder::setTargetBitrate()
  void setTargetBitrate(int bitsPerSecond, double fps) {
    encoder_.setTargetBitrate(bitsPerSecond, fps);
  }
  /// See TinyH264Encoder::setKeyframeInterval()
  void setKeyframeInterval(int frames) {
    encoder_.setKeyframeInterval(frames);
  }
  /// See TinyH264Encoder::setStride()
  void setStride(int strideY, int strideC) {
    encoder_.setStride(strideY, strideC);
  }
  /// See TinyH264Encoder::setPackedStride()
  void setPackedStride(int stride) { encoder_.setPackedStride(stride); }

  /// Pre-sizes the internal bitstream scratch buffer encodeFrame() and its
  /// color-format siblings encode into before writing the result to
  /// setOutput() - call before the first encode call to avoid paying for
  /// the automatic grow-and-retry on it (see class comment). Shrinks are
  /// ignored (the buffer never releases memory on its own).
  void setBitstreamBufferSize(size_t size) {
    if (size > bitstream.size()) bitstream.resize(size);
  }

  /// See TinyH264Encoder::begin()
  bool begin(bool reserveColorConversionScratch = false) {
    encoder_.begin(reserveColorConversionScratch);
    return true;
  }
  /// See TinyH264Encoder::end()
  void end() { encoder_.end(); }

  /// The QP actually used by the most recent encodeFrame()-family call.
  int lastQp() const { return encoder_.lastQp(); }

  /// See TinyH264Encoder::encodeFrame() - writes the result to setOutput()
  /// instead of a caller-supplied buffer; returns the number of bytes
  /// written (0 if nothing was, e.g. setSize()/setOutput() was never
  /// called).
  size_t encodeFrame(const uint8_t *srcY, const uint8_t *srcU,
                      const uint8_t *srcV) {
    return writeOut([&](uint8_t *dst, size_t cap) {
      return encoder_.encodeFrame(srcY, srcU, srcV, dst, cap);
    });
  }
  /// See TinyH264Encoder::encodeFrameRgb888()
  size_t encodeFrameRgb888(const uint8_t *rgb) {
    return writeOut([&](uint8_t *dst, size_t cap) {
      return encoder_.encodeFrameRgb888(rgb, dst, cap);
    });
  }
  /// See TinyH264Encoder::encodeFrameRgb666()
  size_t encodeFrameRgb666(const uint8_t *rgb666) {
    return writeOut([&](uint8_t *dst, size_t cap) {
      return encoder_.encodeFrameRgb666(rgb666, dst, cap);
    });
  }
  /// See TinyH264Encoder::encodeFrameRgb565()
  size_t encodeFrameRgb565(const uint16_t *rgb565) {
    return writeOut([&](uint8_t *dst, size_t cap) {
      return encoder_.encodeFrameRgb565(rgb565, dst, cap);
    });
  }
  /// See TinyH264Encoder::encodeFrameYuv422()
  size_t encodeFrameYuv422(const uint8_t *yuyv) {
    return writeOut([&](uint8_t *dst, size_t cap) {
      return encoder_.encodeFrameYuv422(yuyv, dst, cap);
    });
  }

  /// Direct access to the wrapped TinyH264Encoder, e.g. for its
  /// width()/height()/y()/u()/v() reconstructed-picture accessors.
  tinyh264::TinyH264Encoder<Allocator> &driver() { return encoder_; }

 protected:
  /// Number of times to double the bitstream buffer and retry an
  /// undersized encode call before giving up (see class comment).
  static constexpr int kMaxGrowRetries = 4;

  tinyh264::TinyH264Encoder<Allocator> encoder_;
  Print *p_out = nullptr;
  Vector<uint8_t> bitstream;

  template <typename Fn>
  size_t writeOut(Fn encodeInto) {
    if (p_out == nullptr) return 0;
    if (bitstream.size() == 0) bitstream.resize(kDefaultBitstreamBufferSize);
    size_t n = encodeInto(bitstream.data(), bitstream.size());
    for (int attempt = 0; n == 0 && attempt < kMaxGrowRetries; ++attempt) {
      bitstream.resize(bitstream.size() * 2);
      n = encodeInto(bitstream.data(), bitstream.size());
    }
    if (n > 0) p_out->write(bitstream.data(), n);
    return n;
  }
};

}  // namespace audio_tools
