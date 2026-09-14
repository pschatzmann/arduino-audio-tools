/*
 * Author: Phil Schatzmann
 * Video Information Structure for RTSP Video Streaming
 */

#pragma once
#include "AudioTools/CoreAudio/AudioTypes.h"

namespace audio_tools {

/**
 * @brief Video format enumeration for different pixel formats
 *
 * @note Scoped (and named RTSPVideoFormat, not VideoFormat) to avoid
 * colliding with the unrelated audio_tools::VideoFormat enum in
 * AudioTools/Video/VideoOutput.h (used by the container muxers/demuxers) -
 * both live in the audio_tools namespace, and a translation unit that
 * needs both RTSP.h and a container header (e.g. to mux audio+video into
 * MPEG-TS before streaming it over RTSP) would otherwise hit a
 * redefinition error.
 */
enum class RTSPVideoFormat {
  RGB24,     ///< 24-bit RGB (8 bits per channel)
  RGB565,    ///< 16-bit RGB (5-6-5 bits per channel)
  YUV420,    ///< YUV 4:2:0 planar format
  JPEG,      ///< JPEG compressed format
  MJPEG,     ///< Motion JPEG format
  GRAYSCALE, ///< 8-bit grayscale
  H264       ///< H.264/AVC compressed format
};

/**
 * @brief Video Information Structure
 *
 * Contains all video stream parameters including dimensions, framerate,
 * pixel format, and timing information for RTSP video streaming.
 * This structure parallels AudioInfo for video streams.
 *
 * @note Named RTSPVideoInfo (not VideoInfo) - see RTSPVideoFormat's note.
 * @ingroup rtsp
 * @author Phil Schatzmann
 */
struct RTSPVideoInfo {
  uint16_t width = 640;           ///< Video width in pixels
  uint16_t height = 480;          ///< Video height in pixels
  float framerate = 30.0f;        ///< Frames per second
  RTSPVideoFormat format = RTSPVideoFormat::MJPEG; ///< Pixel format
  uint8_t bits_per_pixel = 24;    ///< Bits per pixel (depends on format)
  uint32_t rtp_clock_rate = 90000; ///< RTP clock rate (90kHz for video per RFC)

  RTSPVideoInfo() = default;

  RTSPVideoInfo(uint16_t w, uint16_t h, float fps = 30.0f,
            RTSPVideoFormat fmt = RTSPVideoFormat::MJPEG, uint8_t bpp = 24)
    : width(w), height(h), framerate(fps), format(fmt), bits_per_pixel(bpp) {}

  /// Calculate frame size in bytes (uncompressed)
  uint32_t frameSize() const {
    switch (format) {
      case RTSPVideoFormat::RGB24:
        return width * height * 3;
      case RTSPVideoFormat::RGB565:
        return width * height * 2;
      case RTSPVideoFormat::YUV420:
        return (width * height * 3) / 2; // Y + U/2 + V/2
      case RTSPVideoFormat::GRAYSCALE:
        return width * height;
      case RTSPVideoFormat::JPEG:
      case RTSPVideoFormat::MJPEG:
        // Variable size - return max estimate (1/4 compression ratio)
        return (width * height * bits_per_pixel) / (8 * 4);
      case RTSPVideoFormat::H264:
        // Variable size - return a rough estimate (much higher compression
        // ratio than JPEG thanks to inter-frame prediction)
        return (width * height * bits_per_pixel) / (8 * 20);
      default:
        return width * height * (bits_per_pixel / 8);
    }
  }

  /// Calculate RTP timestamp increment per frame
  uint32_t timestampIncrement() const {
    return (uint32_t)(rtp_clock_rate / framerate);
  }

  /// Calculate frame period in microseconds
  uint32_t framePeriodUs() const {
    return (uint32_t)(1000000.0f / framerate);
  }

  /// Get aspect ratio
  float aspectRatio() const {
    return (float)width / (float)height;
  }

  /// Check if format is compressed
  bool isCompressed() const {
    return format == RTSPVideoFormat::JPEG || format == RTSPVideoFormat::MJPEG ||
           format == RTSPVideoFormat::H264;
  }

  /// Convert to string for debugging
  String toString() const {
    String result = "VideoInfo: ";
    result += width;
    result += "x";
    result += height;
    result += " @ ";
    result += framerate;
    result += "fps, ";

    switch (format) {
      case RTSPVideoFormat::RGB24: result += "RGB24"; break;
      case RTSPVideoFormat::RGB565: result += "RGB565"; break;
      case RTSPVideoFormat::YUV420: result += "YUV420"; break;
      case RTSPVideoFormat::JPEG: result += "JPEG"; break;
      case RTSPVideoFormat::MJPEG: result += "MJPEG"; break;
      case RTSPVideoFormat::GRAYSCALE: result += "GRAY"; break;
      case RTSPVideoFormat::H264: result += "H264"; break;
      default: result += "UNKNOWN"; break;
    }

    result += " (";
    result += bits_per_pixel;
    result += "bpp)";
    return result;
  }

  /// Log video info
  void logInfo(const char* source = "VideoInfo") const {
    LOGI("%s: %s", source, toString().c_str());
  }
};

/**
 * @brief Base class for video information support
 *
 * Provides interface for classes that need to work with video information.
 * Similar to AudioInfoSupport but for video streams.
 */
class RTSPVideoInfoSupport {
 public:
  virtual ~RTSPVideoInfoSupport() = default;
  virtual void setVideoInfo(RTSPVideoInfo info) = 0;
  virtual RTSPVideoInfo videoInfo() = 0;
};

} // namespace audio_tools
