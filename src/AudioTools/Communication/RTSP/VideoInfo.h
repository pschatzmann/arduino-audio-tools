/*
 * Author: Phil Schatzmann
 * Video Information Structure for RTSP Video Streaming
 */

#pragma once
#include "AudioTools/CoreAudio/AudioTypes.h"

namespace audio_tools {

/**
 * @brief Video format enumeration for different pixel formats
 */
enum VideoFormat {
  VIDEO_RGB24,     ///< 24-bit RGB (8 bits per channel)
  VIDEO_RGB565,    ///< 16-bit RGB (5-6-5 bits per channel)
  VIDEO_YUV420,    ///< YUV 4:2:0 planar format
  VIDEO_JPEG,      ///< JPEG compressed format
  VIDEO_MJPEG,     ///< Motion JPEG format
  VIDEO_GRAYSCALE  ///< 8-bit grayscale
};

/**
 * @brief Video Information Structure
 * 
 * Contains all video stream parameters including dimensions, framerate,
 * pixel format, and timing information for RTSP video streaming.
 * This structure parallels AudioInfo for video streams.
 * 
 * @ingroup rtsp
 * @author Phil Schatzmann
 */
struct VideoInfo {
  uint16_t width = 640;           ///< Video width in pixels
  uint16_t height = 480;          ///< Video height in pixels
  float framerate = 30.0f;        ///< Frames per second
  VideoFormat format = VIDEO_MJPEG; ///< Pixel format
  uint8_t bits_per_pixel = 24;    ///< Bits per pixel (depends on format)
  uint32_t rtp_clock_rate = 90000; ///< RTP clock rate (90kHz for video per RFC)
  
  VideoInfo() = default;
  
  VideoInfo(uint16_t w, uint16_t h, float fps = 30.0f, 
            VideoFormat fmt = VIDEO_MJPEG, uint8_t bpp = 24) 
    : width(w), height(h), framerate(fps), format(fmt), bits_per_pixel(bpp) {}
  
  /// Calculate frame size in bytes (uncompressed)
  uint32_t frameSize() const {
    switch (format) {
      case VIDEO_RGB24:
        return width * height * 3;
      case VIDEO_RGB565:
        return width * height * 2;
      case VIDEO_YUV420:
        return (width * height * 3) / 2; // Y + U/2 + V/2
      case VIDEO_GRAYSCALE:
        return width * height;
      case VIDEO_JPEG:
      case VIDEO_MJPEG:
        // Variable size - return max estimate (1/4 compression ratio)
        return (width * height * bits_per_pixel) / (8 * 4);
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
    return format == VIDEO_JPEG || format == VIDEO_MJPEG;
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
      case VIDEO_RGB24: result += "RGB24"; break;
      case VIDEO_RGB565: result += "RGB565"; break; 
      case VIDEO_YUV420: result += "YUV420"; break;
      case VIDEO_JPEG: result += "JPEG"; break;
      case VIDEO_MJPEG: result += "MJPEG"; break;
      case VIDEO_GRAYSCALE: result += "GRAY"; break;
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
class VideoInfoSupport {
 public:
  virtual ~VideoInfoSupport() = default;
  virtual void setVideoInfo(VideoInfo info) = 0;
  virtual VideoInfo videoInfo() = 0;
};

} // namespace audio_tools