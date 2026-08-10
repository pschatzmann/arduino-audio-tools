#pragma once

#include "AudioTools/AudioCodecs/ContainerCommon.h"
#include "AudioTools/Video/CodecVideo.h"

namespace audio_tools {

/**
 * @brief Feeds a Muxer (MuxerAVI, MuxerMP4, ...) from a single copy() call
 * - no background Tasks, no mutex, driven entirely by the caller's own
 * loop(). Unlike VideoMuxerWithTasks (which paces itself against
 * wall-clock time so audio and video can run on independent threads),
 * this class derives its own timeline purely from the number of audio
 * samples actually written so far: each copy() call reads one PCM chunk,
 * advances that sample count, then mux/encodes however many video frames
 * are now due (frame_index / fps <= audio_time) before returning - so a
 * caller whose audio source can supply data faster than real time (e.g.
 * reading from a file, not a live mic) builds the whole container faster
 * than real time too, with video staying exactly in sync with the audio
 * actually written rather than wall-clock time (so no drift from a slow
 * video encoder either, unlike a timer-paced approach).
 *
 * setAudioSource() is what drives time, so it's effectively required for
 * the faster-than-real-time benefit; without one, there's no audio time to
 * derive video timing from, so copy() instead paces itself against
 * wall-clock time via VideoFrameSource::videoInfo().fps (like
 * VideoMuxerWithTasks's own video task does), compensating for time spent
 * encoding so the real capture cadence matches the fps written into the
 * Muxer's video track.
 * Configuration (setAudioEncoder()/setAudioInfo()/setVideoSource()/
 * setVideoEncoder()) and what begin() derives/configures automatically
 * work exactly like VideoMuxerWithTasks's own - see that class's comment
 * for the details not repeated here. end() closes the Muxer.
 *
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VideoMuxer {
 public:
  VideoMuxer() = default;
  VideoMuxer(Muxer& muxer) { setMuxer(muxer); }

  /// Defines the Muxer copy() writes to - call before begin().
  void setMuxer(Muxer& muxer) { p_muxer = &muxer; }

  /// Defines the raw PCM source copy() reads from - drives this class's
  /// timeline (see class comment). On its own (no setAudioEncoder()) each
  /// chunk is written to the Muxer as raw PCM - call setAudioInfo() too
  /// in that case.
  void setAudioSource(Stream& pcm) { p_audio = &pcm; }

  /// Defines the AudioSource that we can configure with setAudioInfo()
  void setAudioSource(AudioStream& out) {
    p_audio = &out;
    p_source_audio_info = &out;
    audio_info = out.audioInfo();
  }

  /// Defines the sample_rate/channels/bits_per_sample of setAudioSource()'s
  /// PCM data - only used (instead of AudioEncoder::audioInfo()) when no
  /// setAudioEncoder() was called. Call before begin().
  void setAudioInfo(AudioInfo info) {
    audio_info = info;
    if (p_source_audio_info) {
      p_source_audio_info->setAudioInfo(info);
    } else {
      LOGI("VideoMuxer: setAudioInfo() source not updated");
    }
  }

  /// Defines the (optional) encoder each PCM chunk is handed to -
  /// responsible for framing and writing the encoded result to the Muxer
  /// itself; its setOutput() is pointed at the Muxer by begin(). Leave
  /// unset to write setAudioSource()'s PCM straight through instead (see
  /// setAudioInfo()).
  void setAudioEncoder(AudioEncoder& encoder) { p_audio_encoder = &encoder; }

  /// Defines the video frame provider copy() pulls frames from -
  /// already-encoded unless setVideoEncoder() is also called. Leave
  /// unset to write audio only.
  void setVideoSource(VideoFrameSource& video) { p_video = &video; }

  /// Defines the (optional) encoder each raw picture from setVideoSource()
  /// is handed to - e.g. an H264Encoder (AudioTools/Video/CodecH264.h),
  /// configured as usual (setSize()/setVideoFormat()/...) before passing
  /// it here. Leave unset to write setVideoSource()'s bytes straight
  /// through as already-encoded frames instead.
  void setVideoEncoder(VideoEncoder& encoder) { p_video_encoder = &encoder; }

  /// Configures the Muxer's video/audio tracks from the source/encoder
  /// objects above and calls Muxer::begin() - see VideoMuxerWithTasks's
  /// own begin() comment for exactly what that derives; the only Muxer
  /// setup still needed from the caller is Muxer::setOutput().
  bool begin() {
    if (p_muxer == nullptr) {
      LOGE("VideoMuxer: setMuxer() was not called");
      return false;
    }

    have_video = false;
    if (p_video != nullptr) {
      VideoInfo info = p_video->videoInfo();
      if (info.fps <= 0 || info.width == 0 || info.height == 0) {
        LOGE(
            "VideoMuxer: VideoFrameSource::videoInfo() incomplete "
            "(width=%d height=%d fps=%f)",
            (int)info.width, (int)info.height, info.fps);
        return false;
      }
      MuxerVideoConfig cfg;
      cfg.width = info.width;
      cfg.height = info.height;
      cfg.fps = info.fps;
      cfg.format = p_video_encoder != nullptr
                       ? p_video_encoder->videoInfo().format
                       : info.format;
      p_muxer->setVideoInfo(cfg);
      video_fps = info.fps;
      video_interval_ms = (uint32_t)(1000.0f / info.fps);
      video_sink.begin(*p_muxer);
      have_video = true;
    }

    have_audio = p_audio != nullptr;
    if (have_audio) {
      AudioInfoFormat muxer_audio_info;
      if (p_audio_encoder != nullptr) {
        muxer_audio_info = AudioInfoFormat(audio_info);
        AudioFormat mime_format = fromMime(p_audio_encoder->mime());
        muxer_audio_info.format = mime_format != AudioFormat::UNKNOWN
                                      ? mime_format
                                      : AudioFormat::PCM;
      } else {
        muxer_audio_info = AudioInfoFormat(audio_info);
        muxer_audio_info.format = AudioFormat::PCM;
      }
      p_muxer->setAudioInfo(muxer_audio_info);
      sample_rate = muxer_audio_info.sample_rate;
      bytes_per_sample =
          (muxer_audio_info.bits_per_sample / 8) * muxer_audio_info.channels;
    }

    if (!p_muxer->begin()) {
      LOGE("VideoMuxer: Muxer::begin() failed");
      return false;
    }

    if (have_video && p_video_encoder != nullptr) {
      // routed through video_sink, not *p_muxer directly, so it can
      // determine each frame's real isKeyFrame status (see
      // MuxerVideoSink, ContainerCommon.h) instead of defaulting to true
      p_video_encoder->setOutput(video_sink);
      p_video_encoder->begin();
    }
    if (have_audio && p_audio_encoder != nullptr) {
      p_audio_encoder->setAudioInfo(audio_info);
      p_audio_encoder->setOutput(*p_muxer);
      p_audio_encoder->begin();
    }

    audio_samples_written = 0;
    video_frame_count = 0;
    return true;
  }

  /// Closes the Muxer.
  void end() {
    if (p_muxer != nullptr) p_muxer->end();
  }

  /// With an audio source: reads and writes one PCM chunk, then
  /// mux/encodes however many video frames that much audio now covers
  /// (see class comment), returning the number of audio bytes processed -
  /// 0 once setAudioSource() has no more data, the signal to stop calling
  /// this. Without an audio source: pulls/writes exactly one video frame,
  /// paced against wall-clock time by VideoFrameSource::videoInfo().fps
  /// (see class comment), returning 1 (written) or 0 (setVideoSource() has
  /// none left/wasn't set).
  size_t copy() {
    if (have_audio) return copyAudio();
    if (have_video) return copyVideoPaced() ? 1 : 0;
    return 0;
  }

 protected:
  static constexpr size_t kAudioReadChunkSize = 512;

  Muxer* p_muxer = nullptr;
  Stream* p_audio = nullptr;
  AudioEncoder* p_audio_encoder = nullptr;
  AudioInfo audio_info;
  AudioInfoSupport* p_source_audio_info = nullptr;
  VideoFrameSource* p_video = nullptr;
  VideoEncoder* p_video_encoder = nullptr;
  MuxerVideoSink video_sink;
  bool have_audio = false;
  bool have_video = false;
  float video_fps = 0;
  uint32_t video_interval_ms = 33;
  uint32_t sample_rate = 0;
  uint32_t bytes_per_sample = 0;
  uint64_t audio_samples_written = 0;
  uint32_t video_frame_count = 0;

  size_t copyAudio() {
    uint8_t buf[kAudioReadChunkSize];
    size_t n = p_audio->readBytes(buf, sizeof(buf));
    if (n == 0) return 0;

    p_muxer->setStreamType(StreamContentType::Audio);
    if (p_audio_encoder != nullptr) {
      p_audio_encoder->write(buf, n);
    } else {
      p_muxer->addAudioFrame(buf, n);
    }

    if (bytes_per_sample > 0) audio_samples_written += n / bytes_per_sample;

    // catch up: mux/encode every video frame now due, given the audio
    // time just reached - this, not a timer, is what paces video. Strict
    // '<' (not '<=') so a duration landing exactly on a frame boundary
    // doesn't pull one extra frame for the instant at that boundary.
    if (have_video && sample_rate > 0) {
      double audio_time_s = (double)audio_samples_written / sample_rate;
      while ((double)video_frame_count / video_fps < audio_time_s) {
        if (!copyVideo()) break;  // source exhausted
      }
    }
    return n;
  }

  bool copyVideo() {
    size_t len = 0;
    const uint8_t* data = p_video->nextFrame(len);
    if (data == nullptr || len == 0) return false;

    p_muxer->setStreamType(StreamContentType::Video);
    if (p_video_encoder != nullptr) {
      p_video_encoder->write(data, len);
    } else {
      bool is_key = p_muxer->getVideoInfo().format == VideoFormat::H264
                        ? isH264KeyFrame(data, len)
                        : true;
      p_muxer->addVideoFrame(data, len, is_key);
    }
    video_frame_count++;
    return true;
  }

  /// No-audio fallback: paces copyVideo() against wall-clock time via
  /// video_interval_ms, sleeping only the remainder of the interval left
  /// after encoding - same compensation VideoMuxerWithTasks's video task
  /// applies, so a slow encoder doesn't slip the real capture cadence
  /// behind the fps written into the Muxer's video track.
  bool copyVideoPaced() {
    uint32_t start = millis();
    if (!copyVideo()) return false;
    uint32_t elapsed = millis() - start;
    if (elapsed < video_interval_ms) delay(video_interval_ms - elapsed);
    return true;
  }
};

}  // namespace audio_tools
