#pragma once

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/Video/Video.h"

namespace audio_tools {

/// @brief Shared video track configuration for muxers (MuxerAVI,
/// MuxerMP4) - call before begin().
/// @ingroup video
struct MuxerVideoConfig {
  uint16_t width = 320;
  uint16_t height = 240;
  float fps = 25.0f;
  VideoFormat format = VideoFormat::H264;
  /// Optional: overrides the FOURCC derived from 'format'. AVI-specific
  /// (MuxerMP4 determines its sample entry purely from format and ignores
  /// this). Must point to (at least) 4 characters and stay valid until
  /// begin() is called.
  const char *fourcc = nullptr;
};

/**
 * @brief Common interface for muxers (MuxerAVI, MuxerMP4) that combine an
 * already-encoded video track (and optionally an audio track) into a
 * container written to a Print (a local File to record, or e.g. a
 * network Client to publish a live stream to an HTTP/TCP client). Write
 * code against this interface instead of a concrete class if it should
 * work with either container format.
 *
 * write() (VideoOutput) treats each call as one complete, already-encoded
 * frame for both MuxerAVI and MuxerMP4 - callers must hand over the full
 * frame in a single call, not build it up incrementally across several
 * write() calls. flush() is a no-op for both.
 *
 * @ingroup codecs
 * @ingroup encoder
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class Muxer : public VideoOutput {
 public:
  /// The container's MIME type (e.g. "video/avi", "video/mp4") - useful
  /// for e.g. an HTTP Content-Type header when streaming the muxed
  /// output to a client.
  virtual const char *mime() = 0;

  /// Defines the output: e.g. a local File or a network Client
  virtual void setOutput(Print &out) = 0;

  /// Defines the video track configuration - call before begin()
  virtual void setVideoInfo(MuxerVideoConfig config) = 0;
  /// Provides the video track configuration
  virtual MuxerVideoConfig getVideoInfo() = 0;

  /// Adds an (optional) interleaved audio track, including the codec
  /// selection (the 'format' field) - which AudioFormat values are
  /// actually supported (and what they mean, e.g. default codec if
  /// unset) is container-specific; see the concrete class. Call before
  /// begin().
  virtual void setAudioInfo(AudioInfoFormat info) = 0;
  /// Provides read/write access to the audio track's AudioInfoFormat
  virtual AudioInfoFormat &audioInfo() = 0;

  /// Average number of audio samples per video frame, derived from
  /// audioInfo().sample_rate and getVideoInfo().fps - the natural audio
  /// chunk size to write once per video frame if you want to keep both
  /// tracks advancing at roughly the same pace as you write them (not a
  /// hard requirement - see addAudioFrame()/addVideoFrame()). 0 if fps
  /// hasn't been set.
  float getAudioSamplesPerVideoFrame() {
    float fps = getVideoInfo().fps;
    return fps > 0 ? (float)audioInfo().sample_rate / fps : 0;
  }

  /// Writes the container header. Call after configuring video (and
  /// audio, if any) and before writing any frames.
  virtual bool begin() = 0;
  /// Closes the muxer.
  virtual void end() = 0;
  virtual operator bool() = 0;

  /// Selects whether write() feeds the video or the audio track.
  virtual void setStreamType(StreamContentType type) = 0;
  /// The track write() currently targets (see setStreamType())
  virtual StreamContentType streamType() = 0;

  /// Writes one complete, already-encoded video frame (e.g. one H.264
  /// access unit).
  /// @param isKeyFrame hints that the frame is independently decodable
  /// (a sync/seek point) - used where the container format can express
  /// it (e.g. MP4's 'trun' sample flags); ignored where it can't (AVI).
  virtual size_t addVideoFrame(const uint8_t *data, size_t len,
                               bool isKeyFrame = true) = 0;
  /// Writes one complete Motion-JPEG frame (a full, already-encoded JPEG
  /// image).
  virtual size_t addJpegFrame(const uint8_t *data, size_t len) = 0;
  /// Writes one packed 4:2:2 YUV frame (YUY2/YUYV byte order). Expects
  /// exactly width*height*2 bytes.
  virtual size_t addYUV422Frame(const uint8_t *data, size_t len) = 0;
  /// Writes one uncompressed RGB565 (16-bit, 5-6-5) frame. Expects
  /// exactly width*height*2 bytes.
  virtual size_t addRGB565Frame(const uint8_t *data, size_t len) = 0;
  /// Writes one planar 4:2:0 YUV (I420/IYUV) frame. Expects exactly
  /// width*height*3/2 bytes.
  virtual size_t addI420Frame(const uint8_t *data, size_t len) = 0;
  /// Writes one complete audio frame/block. Meaning is container- and
  /// codec-specific (e.g. one raw AAC frame vs. an arbitrary PCM block);
  /// see the concrete class.
  virtual size_t addAudioFrame(const uint8_t *data, size_t len) = 0;

  virtual void flush() {
    if (p_print) p_print->flush();
  }

 protected:
  Print *p_print = nullptr;
};

/**
 * @brief Print sink that writes each frame it receives to a Muxer's video
 * track via addVideoFrame(), determining a real isKeyFrame value instead
 * of relying on that method's isKeyFrame=true default - via
 * isH264KeyFrame() (Video.h) when the Muxer's video track is
 * VideoFormat::H264, otherwise always true (correct for e.g. MJPEG, where
 * every frame is independently decodable).
 *
 * Meant as the setOutput() target for a VideoEncoder whose write() would
 * otherwise stream straight to the Muxer's own generic, format-dispatching
 * write() (which has no isKeyFrame parameter at all) - or for any other
 * already-encoded-frame producer that only has a plain Print to write to.
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MuxerVideoSink : public Print {
 public:
  void begin(Muxer &muxer) { p_muxer = &muxer; }
  size_t write(uint8_t c) override { return write(&c, 1); }
  size_t write(const uint8_t *data, size_t len) override {
    bool is_key = true;
    if (p_muxer->getVideoInfo().format == VideoFormat::H264) {
      is_key = isH264KeyFrame(data, len);
    }
    return p_muxer->addVideoFrame(data, len, is_key);
  }

 protected:
  Muxer *p_muxer = nullptr;
};

/**
 * @brief Common interface for demuxers (DemuxerAVI, DemuxerMP4) that
 * split a container's video and (optional) audio tracks apart. Write
 * code against this interface instead of a concrete class if it should
 * work with either container format.
 * @ingroup codecs
 * @ingroup decoder
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class Demuxer : public ContainerDecoder {
 public:
  /// The container's MIME type (e.g. "video/avi", "video/mp4").
  virtual const char *mime() = 0;

  /// Defines the audio output stream - e.g. an EncodedAudioStream
  /// wrapping an AudioDecoder that matches the track's codec, or any
  /// other Print if you want the raw payload as-is.
  virtual void setOutputAudio(Print &out) = 0;
  /// Defines the video output - e.g. a VideoOutput implementation, or
  /// any other Print if you want the raw payload as-is.
  virtual void setOutputVideo(Print &out) = 0;

  /// Common video info (width/height/format/frame_size/total_file_size)
  virtual VideoInfo getVideoInfo() = 0;
  /// Common audio info (sample_rate/channels/bits_per_sample), extended
  /// with the parsed codec format tag.
  virtual AudioInfoFormat getAudioInfo() = 0;

  /// Overrides the automatic decision of whether a synthetic WAV header
  /// is sent to the audio output before any audio payload - see the
  /// concrete class for the default heuristic.
  virtual void setSendWavHeader(bool flag) = 0;
};

}  // namespace audio_tools
