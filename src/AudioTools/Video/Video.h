#pragma once
#include "AudioTools/CoreAudio/AudioOutput.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "stdint.h"

/**
 * @defgroup video Video
 * @ingroup main
 * @brief Video playback
 */

namespace audio_tools {

/// @brief Which track write() feeds, for muxers (MuxerAVI, MuxerMP4) that
/// double as a plain, generic Print-like sink for both tracks - see
/// setStreamType()/streamType() on those classes.
/// @ingroup video
enum StreamContentType { Audio, Video };

/// @brief Video codec/pixel-format identifier, shared by two unrelated
/// uses: the (single) video stream of a container (DemuxerAVI/MuxerAVI and
/// DemuxerMP4 - much like AudioFormat is shared across the audio demuxers,
/// despite its WAV-flavored name), and the decoded-picture pixel format
/// VideoDecoder::setVideoFormat() selects. H264 is the primary target for
/// muxing; the others are AVI-specific conveniences or decoder-only.
/// - H264/MPEG4: compressed, variable frame size -> use addVideoFrame()
/// - MJPEG: one complete JPEG image per frame -> use addJpegFrame()
/// - RAW: uncompressed 24-bit BGR -> use addVideoFrame()
/// - YUV422: packed 4:2:2 (YUY2/YUYV), 16 bit/pixel -> use addYUV422Frame()
/// - RGB565: uncompressed 16-bit RGB (5-6-5) -> use addRGB565Frame()
/// - RGB666: uncompressed 18-bit RGB, 3 bytes/pixel (each byte's 6
///   significant bits left-justified) - decoder pixel output only, no
///   dedicated Muxer addXxxFrame()
/// - RGB888: uncompressed 24-bit RGB, 3 bytes/pixel, full precision -
///   decoder pixel output only, no dedicated Muxer addXxxFrame()
/// - I420: planar 4:2:0 YUV (aka IYUV/YUV420), 12 bit/pixel ->
///   use addI420Frame()
/// - MPEG1: ISO/IEC 11172-2 compressed video, variable frame size -> use
///   addVideoFrame() (ContainerMPG's MuxerMPG/DemuxerMPG)
/// - UNKNOWN: decoder only - the codec did not match any of the above
/// @ingroup video
enum class VideoFormat {
  H264,
  MJPEG,
  MPEG4,
  RAW,
  YUV422,
  RGB565,
  RGB666,
  RGB888,
  I420,
  MPEG1,
  UNKNOWN
};

/// @brief Fixed per-frame size (bytes) for a raw/uncompressed VideoFormat
/// at the given resolution - 0 for compressed formats (H264/MJPEG/MPEG4) or
/// VideoFormat::UNKNOWN, since their frame size varies per frame.
/// @ingroup video
inline size_t videoFrameSizeBytes(VideoFormat format, uint16_t width,
                                  uint16_t height) {
  size_t px = (size_t)width * (size_t)height;
  switch (format) {
    case VideoFormat::RAW:
    case VideoFormat::RGB666:
    case VideoFormat::RGB888:
      return px * 3;
    case VideoFormat::YUV422:
    case VideoFormat::RGB565:
      return px * 2;
    case VideoFormat::I420:
      return px + px / 2;
    default:
      return 0;
  }
}

/// @brief True if the given Annex-B H.264 access unit contains an IDR
/// slice NAL unit (nal_unit_type 5) - the reliable way to tell a real
/// keyframe/sync-sample apart from a P-frame, since nothing in the byte
/// layout itself says so without inspecting NAL headers. Used e.g. to
/// determine Muxer::addVideoFrame()'s isKeyFrame argument for an
/// already-encoded VideoFormat::H264 frame.
/// @ingroup video
inline bool isH264KeyFrame(const uint8_t *data, size_t len) {
  for (size_t i = 0; i + 3 < len; i++) {
    if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1) {
      if ((data[i + 3] & 0x1F) == 5) return true;  // IDR slice
      i += 2;
    }
  }
  return false;
}

/**
 * @brief Basic video information (width/height/codec/frame size), analogous
 * to AudioInfo - common to both DemuxerAVI and DemuxerMP4, accessible via
 * their getVideoInfo() getter.
 * @ingroup video
 */
struct VideoInfo {
  /// Frame width in pixels
  uint16_t width = 0;
  /// Frame height in pixels
  uint16_t height = 0;
  /// Frames/sec, if known - 0 if not (e.g. not yet determined, or a
  /// VideoFrameSource that doesn't have a fixed rate).
  float fps = 0;
  /// Video codec - VideoFormat::UNKNOWN if not (yet) determined
  VideoFormat format = VideoFormat::UNKNOWN;
  /// Fixed size of a single frame in bytes - only meaningful for
  /// uncompressed formats (RAW/YUV422/RGB565/I420); 0 for compressed
  /// formats (H264/MJPEG/MPEG4), whose frame size varies per frame.
  uint32_t frame_size = 0;
  /// Total size of the container file/stream in bytes, if known. For
  /// DemuxerAVI this comes from the RIFF header's declared size (0 if the
  /// stream is unbounded/streamed). MP4 has no equivalent declared field,
  /// so DemuxerMP4 reports the number of bytes received so far instead
  /// (only equal to the true total once the whole stream has been fed in).
  uint32_t total_file_size = 0;

  bool operator==(const VideoInfo &alt) const {
    return width == alt.width && height == alt.height && fps == alt.fps &&
           format == alt.format && frame_size == alt.frame_size &&
           total_file_size == alt.total_file_size;
  }
  bool operator!=(const VideoInfo &alt) const { return !(*this == alt); }
  /// True if width/height are both defined
  operator bool() const { return width > 0 && height > 0; }
  void logInfo(const char *source = "") {
    LOGI("%s width: %d / height: %d / fps: %f / format: %d / frame_size: %d / "
         "total_file_size: %d",
         source, (int)width, (int)height, fps, (int)format, (int)frame_size,
         (int)total_file_size);
  }
};

/**
 * @brief Pull-based provider of one already-encoded video/image frame at a
 * time - e.g. wraps a camera capture + H264Encoder/MJPEG capture pipeline.
 * Used by VideoMuxerWithTasks's video task, which calls nextFrame() once
 * per iteration, at the rate given by videoInfo().fps. Implementations
 * should produce/capture the frame here directly (not defer it): a slow
 * nextFrame() only delays whatever is pulling from it.
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VideoFrameSource {
 public:
  virtual ~VideoFrameSource() = default;
  /// Provides the next encoded frame: return a pointer to the frame's
  /// bytes (only required to stay valid until the next nextFrame() call)
  /// and set `len` to its size - or return nullptr if no frame is
  /// available yet, in which case the caller skips this iteration.
  virtual const uint8_t *nextFrame(size_t &len) = 0;
  /// Provides the width/height/fps/format (and, if known, frame_size/
  /// total_file_size) of the frames returned by nextFrame() - width,
  /// height, fps and format must be set, since VideoMuxerWithTasks uses
  /// them directly to configure the Muxer it writes to instead of
  /// requiring the caller to duplicate the same values there.
  virtual VideoInfo videoInfo() = 0;

  /// @brief Write the frame to the indicated destination
  bool writeTo(Print &out) {
    size_t len;
    const uint8_t *frame = nextFrame(len);
    if (frame == nullptr || len == 0) return false;
    size_t written = out.write(frame, len);
    return written = len;
  }
};


/**
 * @brief Abstract class for video playback. This class is used to assemble a
 * complete video frame in memory. A video frame is written via one or more
 * write() calls, then finalized with flush() - implementations use flush()
 * to know a frame is complete (there is no separate frame-size hint, unlike
 * a length-prefixed chunk format).
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VideoOutput : public Print {
 public:
  size_t write(uint8_t c) override { return write(&c, 1); }
  virtual size_t write(const uint8_t *data, size_t len) override = 0;
};


/**
 * @brief Logic to Synchronize video and audio output: This is the minimum
 * implementatin which actually does not synchronize, but directly processes the
 * data. No additinal memory is used! Provide your own optimized platform
 * specific implementation
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VideoAudioSync {
 public:
  /// Process the audio data
  virtual void writeAudio(Print *out, uint8_t *data, size_t size) {
    out->write(data, size);
  }

  /// Adds a delay after playing a frame to process with the correct frame rate.
  /// If the playing is too slow we return the mod to select the frames
  virtual void delayVideoFrame(int32_t microsecondsPerFrame,
                               uint32_t time_used_ms) {
    uint32_t delay_ms = microsecondsPerFrame / 1000;
    delay(delay_ms);
  }
};

/**
 * @brief Logic to Synchronize video and audio output: we use a buffer to store
 * the audio and instead of delaying the frames with delay() we play audio. The
 * bufferSize defines the audio buffer in bytes. The correctionMs is used to
 * slow down or speed up the playback of the video to prevent any audio buffer
 * underflows.
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VideoAudioBufferedSync : public VideoAudioSync {
 public:
  VideoAudioBufferedSync(int bufferSize, int correctionMs) {
    ring_buffer.resize(bufferSize);
    correction_ms = correctionMs;
  }

  /// Process the audio data
  void writeAudio(Print *out, uint8_t *data, size_t size) {
    p_out = out;
    if (ring_buffer.availableForWrite() < size) {
      int bytes_to_play = size - ring_buffer.availableForWrite();
      uint8_t audio[bytes_to_play];
      ring_buffer.readArray(audio, bytes_to_play);
      p_out->write(audio, bytes_to_play);
    }
    size_t written = ring_buffer.writeArray(data, size);
    assert(written = size);
  }

  /// Adds a delay after playing a frame to process with the correct frame rate.
  /// If the playing is too slow we return the mod to select the frames
  void delayVideoFrame(int32_t microsecondsPerFrame, uint32_t time_used_ms) {
    uint32_t delay_ms = microsecondsPerFrame / 1000;
    uint64_t timeout = millis() + delay_ms + correction_ms;
    uint8_t audio[8];
    // output audio
    while (millis() < timeout) {
      ring_buffer.readArray(audio, 8);
      p_out->write(audio, 8);
    }
  }

 protected:
  RingBuffer<uint8_t> ring_buffer{0};
  Print *p_out = nullptr;
  int correction_ms = 0;
};

}  // namespace audio_tools
