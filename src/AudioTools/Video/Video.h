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

/// @brief Video codec identifier for the (single) video stream of a
/// container, shared by DemuxerAVI/MuxerAVI and DemuxerMP4 (much like
/// AudioFormat is shared across the audio demuxers, despite its
/// WAV-flavored name). H264 is the primary target for both; the others
/// are AVI-specific conveniences.
/// - H264/MPEG4: compressed, variable frame size -> use addVideoFrame()
/// - MJPEG: one complete JPEG image per frame -> use addJpegFrame()
/// - RAW: uncompressed 24-bit BGR -> use addVideoFrame()
/// - YUV422: packed 4:2:2 (YUY2/YUYV), 16 bit/pixel -> use addYUV422Frame()
/// - RGB565: uncompressed 16-bit RGB (5-6-5) -> use addRGB565Frame()
/// - I420: planar 4:2:0 YUV (aka IYUV), 12 bit/pixel -> use addI420Frame()
/// - UNKNOWN: decoder only - the codec did not match any of the above
/// @ingroup video
enum class VideoFormat { H264, MJPEG, MPEG4, RAW, YUV422, RGB565, I420, UNKNOWN };

/// @brief Fixed per-frame size (bytes) for a raw/uncompressed VideoFormat
/// at the given resolution - 0 for compressed formats (H264/MJPEG/MPEG4) or
/// VideoFormat::UNKNOWN, since their frame size varies per frame.
/// @ingroup video
inline size_t videoFrameSizeBytes(VideoFormat format, uint16_t width,
                                  uint16_t height) {
  size_t px = (size_t)width * (size_t)height;
  switch (format) {
    case VideoFormat::RAW:
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
    return width == alt.width && height == alt.height &&
           format == alt.format && frame_size == alt.frame_size &&
           total_file_size == alt.total_file_size;
  }
  bool operator!=(const VideoInfo &alt) const { return !(*this == alt); }
  /// True if width/height are both defined
  operator bool() const { return width > 0 && height > 0; }
  void logInfo(const char *source = "") {
    LOGI("%s width: %d / height: %d / format: %d / frame_size: %d / "
         "total_file_size: %d",
         source, (int)width, (int)height, (int)format, (int)frame_size,
         (int)total_file_size);
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
 * @brief Simple in-memory VideoOutput sink that assembles a single video
 * frame of at most a fixed maximum size. Connect it to any write()/flush()
 * producer (e.g. a decoder), then, once flush() has been called, read the
 * assembled frame back via data() and available() (or register a
 * setOnEnd() callback to be notified instead of polling).
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VideoFrame : public VideoOutput {
 public:
  /// Callback signature for setOnEnd(): invoked from flush() with a
  /// reference to the VideoFrame whose frame was just completed.
  using VideoFrameCallback = void (*)(VideoFrame &frame);

  /// @param maxSize maximum frame size in bytes that this instance can hold
  VideoFrame(size_t maxSize) : max_size(maxSize) { buffer.resize(max_size); }

  /// Registers a callback that is invoked from flush(), once the frame has
  /// been fully assembled, receiving a reference to this VideoFrame so it
  /// can read data()/available().
  void setOnEnd(VideoFrameCallback callback) { on_end_cb = callback; }

  /// Appends data to the frame; truncates (rather than overflowing the
  /// buffer) if it would exceed maxSize. Returns the number of bytes
  /// actually written.
  size_t write(const uint8_t *data, size_t len) override {
    size_t to_write = pos + len <= max_size ? len : max_size - pos;
    memcpy(buffer.data() + pos, data, to_write);
    pos += to_write;
    return to_write;
  }

  /// Finalizes the frame: invokes the setOnEnd() callback (if any) with a
  /// reference to this VideoFrame, then resets the write position so the
  /// next write() call starts a fresh frame.
  void flush() override {
    if (on_end_cb != nullptr) on_end_cb(*this);
    pos = 0;
  }

  /// Number of bytes currently assembled in the frame
  size_t available() { return pos; }

  /// Manually declares how many bytes of the frame are valid, e.g. after
  /// writing directly into data() from an external source (DMA buffer,
  /// camera driver, ...) instead of going through write(). Clamped to
  /// size().
  void setAvailable(size_t available) {
    pos = available <= max_size ? available : max_size;
  }

  /// Pointer to the assembled frame data
  uint8_t *data() { return buffer.data(); }

  /// Maximum frame size (bytes) this instance can hold
  size_t size() { return max_size; }

 protected:
  Vector<uint8_t> buffer;
  size_t max_size;
  size_t pos = 0;
  VideoFrameCallback on_end_cb = nullptr;
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
