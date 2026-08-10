#pragma once

#include "AudioTools/Video/Video.h"
#include "H264Decoder.h"
#include "H264Encoder.h"

/**
 * @defgroup h264esp32s3 H264ESP32S3
 * @ingroup codecs
 * @ingroup video
 * @brief H.264 encoding/decoding on ESP32-S3 using
 * https://github.com/pschatzmann/ESP32S3-h264 (esp_h264, hardware-assisted
 * where available). ESP32-S3 only - H264Config.h in that library fails the
 * build on any other target.
 */

namespace audio_tools {

/**
 * @brief H.264 video decoder for ESP32-S3: wraps esp_h264::H264Decoder
 * (https://github.com/pschatzmann/ESP32S3-h264) as a VideoOutput, so it can
 * be plugged directly into e.g. DemuxerAVI::setOutputVideo()/
 * DemuxerMP4::setOutputVideo() - the same role H264Decoder (CodecH264.h,
 * the portable TinyH264-based decoder) plays, for boards where the
 * ESP32-S3-specific esp_h264 backend is preferred instead. Consumes an
 * Annex-B H.264 bitstream via write() and, once a complete picture has been
 * decoded, writes it - converted to setPixelFormat()'s format (RGB565 by
 * default, the common TFT wire format) - to the Print target configured via
 * setOutput(), one write() call per decoded picture.
 *
 * write()/flush() implement the VideoOutput contract (see Video.h): write()
 * may be called one or more times per frame - bytes are forwarded to the
 * wrapped decoder's decode() immediately, which invokes the frame callback
 * synchronously for every completed picture found in that call, not
 * deferred to flush(). flush() is a no-op, kept only because some producers
 * (e.g. DemuxerAVI/DemuxerMP4) call it unconditionally after each frame.
 *
 * The Alloc template parameter is forwarded to esp_h264::H264Decoder - see
 * that class's own file comment (H264Decoder.h) for the PSRAM- vs.
 * internal-RAM allocator choice (H264DecoderPSRAM/H264DecoderRAM aliases).
 *
 * @ingroup h264esp32s3
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <typename Alloc = H264_DEFAULT_ALLOCATOR>
class H264DecoderESP32S3 : public VideoOutput {
 public:
  H264DecoderESP32S3() {
    config_ = decoder_.defaultConfig();
    config_.output_format = ESP_H264_RAW_FMT_RGB565_LE;
    config_.frame_callback = [this](const uint8_t *frame, uint32_t w,
                                     uint32_t h, esp_h264_raw_format_t fmt) {
      if (p_out == nullptr) return;
      size_t bytes = (size_t)(w * h * ESP_H264_GET_BPP_BY_PIC_TYPE(fmt));
      p_out->write(frame, bytes);
    };
  }

  /// Defines the target the decoded picture is written to, one write()
  /// call per decoded picture, in the format selected via
  /// setPixelFormat() (RGB565 by default). Call before begin().
  void setOutput(Print &out) { p_out = &out; }

  /// Selects the pixel format written to setOutput()'s target - see
  /// esp_h264_types.h's ESP_H264_RAW_FMT_* for the options this decoder
  /// supports (RGB565_LE, the default, or I420). Call before begin().
  void setPixelFormat(esp_h264_raw_format_t format) {
    config_.output_format = format;
  }

  /// Input buffer size (bytes) the wrapped decoder allocates for its own
  /// copy of write()'s data - see esp_h264::H264Decoder::Config::
  /// input_buffer_size. Call before begin().
  void setInputBufferSize(size_t size) { config_.input_buffer_size = size; }

  /// Output buffer size (bytes) the wrapped decoder allocates for the
  /// decoded/converted picture - must be at least width*height*bytes-
  /// per-pixel for setPixelFormat()'s format. Call before begin().
  void setOutputBufferSize(size_t size) { config_.output_buffer_size = size; }

  /// Initializes the decoder - see esp_h264::H264Decoder::begin().
  bool begin() { return decoder_.begin(config_); }

  /// Releases the decoder's resources - see esp_h264::H264Decoder::end().
  void end() { decoder_.end(); }

  /// Feeds one chunk of Annex-B H.264 data - may be called more than once
  /// per frame. Decodes immediately, invoking setOutput()'s Print once per
  /// completed picture from within this call (not deferred to flush()).
  size_t write(const uint8_t *data, size_t len) override {
    decoder_.decode(data, len);
    return len;
  }

  /// No-op - kept to satisfy the VideoOutput/Print contract; decoding
  /// already happens synchronously in write() (see class comment).
  void flush() override {}

  /// Number of frames successfully decoded since begin().
  uint32_t frameCount() const { return decoder_.getFrameCount(); }
  /// Number of decode errors since begin().
  uint32_t decodeErrors() const { return decoder_.getDecodeErrors(); }

  /// Direct access to the wrapped esp_h264::H264Decoder.
  esp_h264::H264Decoder<Alloc> &driver() { return decoder_; }

 protected:
  esp_h264::H264Decoder<Alloc> decoder_;
  typename esp_h264::H264Decoder<Alloc>::Config config_;
  Print *p_out = nullptr;
};

/**
 * @brief H.264 video encoder for ESP32-S3: wraps esp_h264::H264Encoder
 * (https://github.com/pschatzmann/ESP32S3-h264) - the ESP32-S3-specific
 * counterpart of H264Encoder (CodecH264.h, the portable TinyH264-based
 * encoder). Configure via setSize()/setFrameRate()/setBitrate()/
 * setGop()/setQpRange(), then feed raw pictures via encodeFrame()
 * (I420)/encodeFrameRgb565()/encodeFrameYuv422() - each writes the
 * resulting Annex-B bitstream straight to the Print target passed to
 * setOutput() (the underlying esp_h264::H264Encoder methods already take a
 * Print& and handle the write, retrying on partial writes - see
 * H264Encoder.h's encode()).
 *
 * Unlike esp_h264::H264Encoder itself, this wrapper does not drive an
 * attached camera (its use_camera/pin/captureH264() surface) - it only
 * covers the encode-a-buffer-you-already-have path, matching H264Encoder's
 * (CodecH264.h) scope; use esp_h264::H264Encoder directly (via driver())
 * if you want its built-in camera capture loop instead.
 *
 * The Alloc template parameter is forwarded to esp_h264::H264Encoder - see
 * that class's own file comment (H264Encoder.h) for the PSRAM- vs.
 * internal-RAM allocator choice (H264EncoderPSRAM/H264EncoderRAM aliases).
 *
 * @ingroup h264esp32s3
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <typename Alloc = H264_DEFAULT_ALLOCATOR>
class H264EncoderESP32S3 {
 public:
  H264EncoderESP32S3() { config_ = encoder_.defaultConfig(); }

  /// Defines the target each encodeFrame()-family call writes its encoded
  /// bitstream to.
  void setOutput(Print &out) { p_out = &out; }

  /// Picture size - must match the raw frame data passed to the
  /// encodeFrame()-family methods. Call before begin().
  void setSize(int width, int height) {
    config_.width = width;
    config_.height = height;
  }
  /// Target frames/sec - also the default GOP size (see setGop()) unless
  /// overridden. Call before begin().
  void setFrameRate(int fps) { config_.fps = fps; }
  /// Target bitrate (bits/sec) - defaults to width*height*fps*30/100 if
  /// never called. Call before begin().
  void setBitrate(int bitsPerSecond) { config_.bitrate = bitsPerSecond; }
  /// Group-of-Pictures size (keyframe interval, in frames) - defaults to
  /// setFrameRate()'s fps if never called. Call before begin().
  void setGop(int gop) { config_.gop = gop; }
  /// Quantizer parameter range [0, 51] the encoder's rate control stays
  /// within - lower is better quality/higher bitrate. Defaults to
  /// [28, 30] if never called. Call before begin().
  void setQpRange(int qpMin, int qpMax) {
    config_.qp_min = qpMin;
    config_.qp_max = qpMax;
  }
  /// Output buffer size (bytes) the wrapped encoder allocates for the
  /// encoded bitstream - defaults to one uncompressed I420 frame's worth
  /// if never called. Call before begin().
  void setOutputBufferSize(size_t size) { config_.outBufferSize = size; }

  /// Initializes the encoder - see esp_h264::H264Encoder::begin().
  bool begin() { return encoder_.begin(config_); }

  /// Releases the encoder's resources - see esp_h264::H264Encoder::end().
  void end() { encoder_.end(); }

  /// Encodes one I420 (YUV420 planar: Y, then U, then V, no row padding)
  /// picture and writes the result to setOutput() - `len` must be at least
  /// width*height*3/2 (see esp_h264::H264Encoder::encode()).
  bool encodeFrame(const uint8_t *i420, size_t len) {
    if (p_out == nullptr) return false;
    return encoder_.encode(i420, len, *p_out);
  }
  /// Encodes one RGB565 (16-bit, 5-6-5 packed, little-endian) picture -
  /// converted internally to I420 before encoding - and writes the result
  /// to setOutput(). `len` must be at least width*height*2.
  bool encodeFrameRgb565(const uint8_t *rgb565, size_t len) {
    if (p_out == nullptr) return false;
    return encoder_.encodeRGB565(rgb565, len, *p_out);
  }
  /// Encodes one YUYV-order packed YUV 4:2:2 picture (the common camera-
  /// module convention) - converted internally to I420 before encoding -
  /// and writes the result to setOutput(). `len` must be at least
  /// width*height*2.
  bool encodeFrameYuv422(const uint8_t *yuyv, size_t len) {
    if (p_out == nullptr) return false;
    return encoder_.encodeYUV422(yuyv, len, *p_out);
  }

  /// Direct access to the wrapped esp_h264::H264Encoder, e.g. for its
  /// camera-capture surface (begin() with use_camera/pin fields set,
  /// captureH264()) - not exposed through this wrapper.
  esp_h264::H264Encoder<Alloc> &driver() { return encoder_; }

 protected:
  esp_h264::H264Encoder<Alloc> encoder_;
  typename esp_h264::H264Encoder<Alloc>::Config config_;
  Print *p_out = nullptr;
};

}  // namespace audio_tools
