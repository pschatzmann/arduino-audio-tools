#pragma once
#include "AudioTools/Video/Video.h"

namespace audio_tools {

/**
 * @brief Common interface for video decoders (e.g. H264Decoder,
 * H264DecoderESP32S3 - CodecH264.h/CodecH264ESP32S3.h) - standardizes
 * lifecycle (begin()/end()), the Print target decoded pictures are
 * written to, and the pixel format they're written in (setVideoFormat()),
 * on top of VideoOutput's write()/flush() (the encoded-bitstream input
 * side, inherited unchanged). Concrete decoders may still expose their
 * own additional config knobs beyond this shared surface.
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VideoDecoder : public VideoOutput {
 public:
  virtual ~VideoDecoder() = default;
  /// Defines the target each decoded picture is written to.
  virtual void setOutput(Print &out) = 0;
  /// Selects the pixel format written to setOutput()'s target - e.g.
  /// VideoFormat::RGB565 (the common TFT wire format), RGB666/RGB888 for
  /// higher color depth displays, or I420 to pass the decoded planes
  /// through unconverted. Not every decoder backend supports every value
  /// (e.g. RGB666/RGB888 are TinyH264-only, not available on the
  /// esp_h264 backend) - unsupported values are logged and ignored (the
  /// previously selected format stays in effect); see the concrete
  /// class for exactly which ones it supports. Call before begin().
  virtual void setVideoFormat(VideoFormat format) = 0;
  /// Reports the format/dimensions of the picture written to
  /// setOutput()'s target - VideoInfo::format is always the format most
  /// recently selected via setVideoFormat() (RGB565 if never called),
  /// the reliable way to determine it (rather than assuming); width/
  /// height reflect the most recently decoded picture, 0 before any
  /// picture has been decoded.
  virtual VideoInfo videoInfo() = 0;
  /// Initializes the decoder (allocates its picture buffers, etc).
  virtual bool begin() = 0;
  /// Releases the decoder's resources.
  virtual void end() = 0;
};

/**
 * @brief Common interface for video encoders (e.g. H264Encoder,
 * H264EncoderESP32S3 - CodecH264.h/CodecH264ESP32S3.h) - standardizes
 * lifecycle (begin()/end()), the Print target the encoded bitstream is
 * written to, the raw-picture input format (setVideoFormat()), and a
 * single write() that encodes one picture in that format and writes the
 * result to setOutput()'s target - one write() call per picture, mirroring
 * VideoOutput's write()-per-frame convention on the decoder side. Concrete
 * encoders may still expose their own additional config knobs (e.g.
 * bitrate/QP) beyond this shared surface.
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VideoEncoder {
 public:
  virtual ~VideoEncoder() = default;
  /// Defines the target the encoded bitstream is written to.
  virtual void setOutput(Print &out) = 0;
  /// Selects the raw picture format write() expects - e.g.
  /// VideoFormat::I420 (planar Y/U/V, concatenated in one buffer),
  /// YUV422 (packed YUYV), RGB565/RGB666/RGB888 (packed). Not every
  /// encoder backend supports every value - unsupported values are
  /// logged and ignored (the previously selected format stays in
  /// effect); see the concrete class for exactly which ones it supports.
  /// Call before the first write().
  virtual void setVideoFormat(VideoFormat format) = 0;
  /// Reports the format/dimensions of the bitstream written to
  /// setOutput()'s target - VideoInfo::format is the actual encoded
  /// codec (e.g. VideoFormat::H264), NOT setVideoFormat()'s raw input
  /// format, the reliable way to determine it (rather than assuming);
  /// width/height match the encoder's configured picture size (e.g.
  /// setSize()), 0 if that was never called.
  virtual VideoInfo videoInfo() = 0;
  /// Initializes the encoder (allocates its picture buffers, etc).
  virtual bool begin() = 0;
  /// Releases the encoder's resources.
  virtual void end() = 0;
  /// Encodes one raw picture (in setVideoFormat()'s format, sized for
  /// the encoder's configured width/height) and writes the result to
  /// setOutput()'s target, returning the number of bytes written (0 if
  /// nothing was, e.g. setOutput()/size wasn't configured, or the
  /// format is unsupported).
  virtual size_t write(const uint8_t *data, size_t len) = 0;
};

}  // namespace audio_tools
