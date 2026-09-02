#pragma once
#include "AudioTools/CoreAudio/AudioOutput.h"
#include "stdint.h"

namespace audio_tools {

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

  bool operator==(const VideoInfo& alt) const {
    return width == alt.width && height == alt.height && fps == alt.fps &&
           format == alt.format && frame_size == alt.frame_size &&
           total_file_size == alt.total_file_size;
  }
  bool operator!=(const VideoInfo& alt) const { return !(*this == alt); }
  /// True if width/height are both defined
  operator bool() const { return width > 0 && height > 0; }
  void logInfo(const char* source = "") {
    LOGI(
        "%s width: %d / height: %d / fps: %f / format: %d / frame_size: %d / "
        "total_file_size: %d",
        source, (int)width, (int)height, fps, (int)format, (int)frame_size,
        (int)total_file_size);
  }
};

/// @brief Provider of VideoInfo (width/height/fps/format) for whoever needs
/// to size buffers/panel setup against it - e.g. a Demuxer (already parsed
/// from the container's own metadata) handed to VideoOutput::
/// setVideoInfoSource().
/// @ingroup video
class VideoInfoSource {
 public:
  virtual VideoInfo videoInfo() = 0;
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
class VideoOutput {
 public:
  virtual size_t write(const uint8_t* data, size_t len) = 0;
  /// Finalizes the frame most recently written via one or more write()
  /// calls - see class comment. Default no-op for implementations that
  /// display/decode synchronously in write() instead.
  virtual void flush() {}
  /// Hint to skip the expensive part of displaying the next frame(s) (e.g.
  /// the panel refresh) while still accepting and fully processing write()
  /// calls - used to recover from falling behind the playback schedule
  /// without breaking a codec's decode state (e.g. H.264 inter-prediction
  /// reference chain, which requires every frame to still be decoded even
  /// if it's never shown). Default no-op: implementations that can't skip
  /// rendering cheaply just ignore it and always render.
  virtual void setSkipRender(bool skip) {}

  /// True if `data` (one complete encoded frame, as handed to write())
  /// is a keyframe/sync-sample - self-contained, decodable without any
  /// earlier frame. Used e.g. by PacedVideoOutput to decide which
  /// frames are safe to drop, and whether it's safe to resume decoding
  /// after abandoning a backlog (see its own class comment). Default
  /// false: a plain VideoOutput doesn't know or care about codec
  /// structure - override this in a decoder for the bitstream format it
  /// actually parses (see H264Decoder/H264DecoderESP32S3's
  /// isH264KeyFrame()-based override, MPGDecoder's isMpeg1KeyFrame()-
  /// based one). Getting this right matters beyond bookkeeping: a target
  /// whose frames are never recognized as keyframes can leave a caller
  /// like PacedVideoOutput unable to ever resume after a resync.
  virtual bool isKeyFrame(const uint8_t* data, size_t len) { return false; }

  /// Optional: returns the time (ms) spent in the last write() call
  virtual uint32_t getWriteTimeMs() const { return 0; }

  /// True if the most recent write()+flush() call actually produced a
  /// displayable picture - default true, matching every synchronous
  /// decoder (H264Decoder, MJPEGDecoder, ...), which always decodes and
  /// pushes pixels fully within that one call. Override this only if
  /// your decoder can legitimately accept/decode a frame's bytes without
  /// emitting a picture during that same call - e.g. MPGDecoder, whose
  /// B-picture display-order reordering can hold a just-decoded picture
  /// back and instead emit an earlier one (or nothing at all) from a
  /// given write(), see its own override. Used by PacedVideoOutput to
  /// avoid counting/timing a call that did no real rendering work as a
  /// rendered frame - without this, its outputFPS()/frameCountI()/
  /// frameCountP()/avgFrameMs() would overcount for such a decoder.
  virtual bool hadOutput() const { return true; }

  /// Optional: registers where width/height/fps/format (VideoInfo) come
  /// from - e.g. the demuxer feeding this output, so it can size its own
  /// buffers/panel setup without the caller having to duplicate that call
  /// per sketch (VideoPlayer::begin() does this automatically for
  /// whichever VideoOutput it was given). Default no-op: only
  /// implementations that actually need VideoInfo (OutputTinyGPU/
  /// OutputOpenCV/OutputTFT_eSPI) override this.
  virtual void setVideoInfoSource(VideoInfoSource& source) {}

  /// Optional: sum of time (ms) spent purely decoding (excluding any
  /// surrounding convert/render/SPI work a subclass's write() also does)
  /// since begin() - see H264Decoder's own override for the only current
  /// implementation. Default 0: only meaningful for a decoder that
  /// separates decode time from render time internally: PacedVideoOutput::
  /// logTo() prints a decode-vs-render split under "avg decode ms:" only
  /// when this returns nonzero.
  virtual uint64_t totalDecodeMs() const { return 0; }
};

}  // namespace audio_tools
