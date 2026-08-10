#pragma once

#include "AudioTools/AudioCodecs/ContainerCommon.h"
#include "AudioTools/Video/CodecVideo.h"

#ifdef __linux__
#include "AudioTools/Concurrency/Desktop.h"
#else
#include "AudioTools/Concurrency/RTOS.h"
#endif

namespace audio_tools {

/**
 * @brief Feeds a Muxer (MuxerAVI, MuxerMP4, ...) from two background Tasks
 * instead of a single loop()/copy() call - one per track, each write
 * wrapped in a LockGuard over a shared mutex (real by default; override
 * via setMutex()), since a Muxer is not safe to call from two threads
 * concurrently.
 *
 * Audio (setAudioSource() + optional setAudioEncoder()): reads PCM from a
 * Stream expected to deliver data synchronously, at the real capture rate
 * (e.g. a blocking I2S read) - that's this task's only pacing, no
 * timer/delay() of its own. With an encoder, each chunk is handed to it
 * (framed and written to the Muxer by the encoder itself); without one,
 * each chunk is written to the Muxer as raw PCM directly (call
 * setAudioInfo() in that case).
 *
 * Video (setVideoSource() + optional setVideoEncoder()): pulls one frame
 * per iteration, timed by VideoFrameSource::videoInfo().fps. With an
 * encoder, each raw picture is passed to VideoEncoder::write(); without
 * one, the frame is written to the Muxer as-is (already-encoded).
 *
 * begin() derives both Muxer tracks from the source/encoder objects above
 * (VideoEncoder::videoInfo().format decides the real container format,
 * not VideoFrameSource's, since that describes the raw picture when an
 * encoder is used) and calls Muxer::begin() itself - the only Muxer setup
 * still needed from the caller is Muxer::setOutput(). end() stops both
 * tasks and closes the Muxer.
 *
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VideoMuxerWithTasks {
 public:
  VideoMuxerWithTasks() = default;
  VideoMuxerWithTasks(Muxer &muxer) { setMuxer(muxer); }

  /// Defines the Muxer both tasks write to - call before begin().
  void setMuxer(Muxer &muxer) { p_muxer = &muxer; }

  /// Defines the raw PCM source the audio task reads from - expected to
  /// deliver data synchronously, at the real capture rate (see class
  /// comment). On its own (no setAudioEncoder()) each chunk is written to
  /// the Muxer as raw PCM - call setAudioInfo() too in that case.
  void setAudioSource(Stream &pcm) { p_audio = &pcm; }

  /// Defines the AudioSource that we can configure with setAudioInfo() 
  void setAudioSource(AudioStream &out) { 
    p_audio = &out;
    p_source_audio_info = &out;
    audio_info = out.audioInfo();
   }

  /// Defines the sample_rate/channels/bits_per_sample of setAudioSource()'s
  /// PCM data - only used (instead of AudioEncoder::audioInfo()) when no
  /// setAudioEncoder() was called, i.e. the PCM is written to the Muxer
  /// as-is. Call before begin().
  void setAudioInfo(AudioInfo info) { 
    audio_info = info;
    if (p_source_audio_info) {
      p_source_audio_info->setAudioInfo(info);
    } else {
      LOGI("VideoMuxerWithTasks: setAudioInfo() source not updated");
    } 
  }

  /// Defines the (optional) encoder the audio task hands each PCM chunk
  /// to - responsible for framing and writing the encoded result to the
  /// Muxer itself; its setOutput() is pointed at the Muxer by begin(),
  /// overriding any output previously set on it. Its audioInfo() (set it
  /// on the encoder, as usual, before calling this) and mime() (see
  /// AudioFormat.h's fromMime() - best-effort; falls back to
  /// AudioFormat::PCM if it doesn't map) configure the Muxer's audio
  /// track. Leave unset to write setAudioSource()'s PCM straight through
  /// instead (see setAudioInfo()).
  void setAudioEncoder(AudioEncoder &encoder) { p_audio_encoder = &encoder; }

  /// Defines the video frame provider the video task pulls one frame from
  /// per iteration - already-encoded unless setVideoEncoder() is also
  /// called (see class comment). Leave unset to run the audio task only.
  void setVideoSource(VideoFrameSource &video) { p_video = &video; }

  /// Defines the (optional) encoder the video task hands each raw picture
  /// from setVideoSource() to - e.g. an H264Encoder (AudioTools/Video/
  /// CodecH264.h), configured as usual (setSize()/setVideoFormat()/...)
  /// before passing it here. Leave unset to write setVideoSource()'s
  /// bytes straight through as already-encoded frames instead.
  void setVideoEncoder(VideoEncoder &encoder) { p_video_encoder = &encoder; }

  /// Assigns the shared mutex protecting the Muxer from concurrent access
  /// by the audio and video tasks - defaults to a real platform mutex
  /// (see DefaultMutex below), so this is normally only needed to swap in
  /// a different implementation. Call before begin().
  void setMutex(MutexBase &mutex) { p_mutex = &mutex; }

  /// Stack size (words, not bytes)/priority/core applied to both tasks -
  /// see audio_tools::Task's own constructor comment for what each
  /// parameter means and its platform-specific caveats. Call before
  /// begin().
  void setTaskParameters(uint32_t stackSizeWords, uint8_t priority,
                         int core = -1) {
    task_stack_size = stackSizeWords;
    task_priority = priority;
    task_core = core;
  }

  /// Configures the Muxer's video/audio tracks from the source/encoder
  /// objects above, calls Muxer::begin(), then starts the audio and/or
  /// video task - see the class comment for exactly what's still the
  /// caller's own responsibility beforehand.
  bool begin() {
    if (p_muxer == nullptr) {
      LOGE("VideoMuxerWithTasks: setMuxer() was not called");
      return false;
    }

    bool have_video = false;
    if (p_video != nullptr) {
      VideoInfo info = p_video->videoInfo();
      if (info.fps <= 0 || info.width == 0 || info.height == 0) {
        LOGE(
            "VideoMuxerWithTasks: VideoFrameSource::videoInfo() incomplete "
            "(width=%d height=%d fps=%f)",
            (int)info.width, (int)info.height, info.fps);
        return false;
      }
      MuxerVideoConfig cfg;
      cfg.width = info.width;
      cfg.height = info.height;
      cfg.fps = info.fps;
      // with a video encoder, the source's own format describes the raw
      // picture, not the container - query the encoder's own
      // VideoEncoder::videoInfo() instead, the reliable way to determine
      // what actually ends up in the container
      cfg.format = p_video_encoder != nullptr
                       ? p_video_encoder->videoInfo().format
                       : info.format;
      p_muxer->setVideoInfo(cfg);
      video_interval_ms = (uint32_t)(1000.0f / info.fps);
      video_sink.begin(*p_muxer);
      have_video = true;
    }

    bool have_audio = p_audio != nullptr;
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
    }

    if (!p_muxer->begin()) {
      LOGE("VideoMuxerWithTasks: Muxer::begin() failed");
      return false;
    }

    bool ok = true;
    if (have_video) {
      if (p_video_encoder != nullptr) {
        // routed through video_sink, not *p_muxer directly, so it can
        // determine each frame's real isKeyFrame status (see
        // MuxerVideoSink) instead of defaulting to true
        p_video_encoder->setOutput(video_sink);
        p_video_encoder->begin();
      }
      ok &= video_task.create("VideoMuxerVideoTask", task_stack_size,
                              task_priority, task_core);
      ok &= video_task.begin([this]() { videoTaskLoop(); });
    }
    if (have_audio) {
      if (p_audio_encoder != nullptr) {
        p_audio_encoder->setAudioInfo(audio_info);
        p_audio_encoder->setOutput(*p_muxer);
        p_audio_encoder->begin();
      }
      ok &= audio_task.create("VideoMuxerAudioTask", task_stack_size,
                              task_priority, task_core);
      ok &= audio_task.begin([this]() { audioTaskLoop(); });
    }
    return ok;
  }

  /// Stops both tasks and closes the Muxer.
  void end() {
    audio_task.end();
    video_task.end();
    if (p_muxer != nullptr) p_muxer->end();
  }

  bool isRunning() { return audio_task.isRunning() || video_task.isRunning(); }

 protected:
  /// Size (bytes) of the PCM chunk read from setAudioSource() and handed
  /// to setAudioEncoder()'s write() per audio task iteration - purely a
  /// throughput/latency knob (the encoder does its own framing on top of
  /// however much data it's given), not a correctness-critical value.
  static constexpr size_t kAudioReadChunkSize = 512;

  /// Real mutex type used for the default_mutex member (see setMutex()) -
  /// audio_tools::Mutex (MutexRTOS) on RTOS platforms, StdMutex on
  /// desktop - both real, so two tasks are protected out of the box
  /// without requiring setMutex().
#ifdef __linux__
  using DefaultMutex = StdMutex;
#else
  using DefaultMutex = Mutex;
#endif

  Muxer *p_muxer = nullptr;
  Stream *p_audio = nullptr;
  AudioEncoder *p_audio_encoder = nullptr;
  AudioInfo audio_info;
  AudioInfoSupport *p_source_audio_info = nullptr;
  VideoFrameSource *p_video = nullptr;
  VideoEncoder *p_video_encoder = nullptr;
  MuxerVideoSink video_sink;
  DefaultMutex default_mutex;
  MutexBase *p_mutex = &default_mutex;
  Task audio_task;
  Task video_task;
  uint32_t task_stack_size = 4096;
  uint8_t task_priority = 2;
  int task_core = -1;
  uint32_t video_interval_ms = 33;

  /// Reads one PCM chunk and either hands it to the audio encoder (which
  /// frames and writes it to the Muxer itself) or, without one, writes it
  /// to the Muxer directly as one raw PCM frame - no delay() on success:
  /// pacing is the PCM Stream's own responsibility (see class comment); a
  /// short fallback delay only kicks in if it returned nothing, so a
  /// Stream that violates that contract (e.g. non-blocking) can't spin
  /// this task at full CPU.
  void audioTaskLoop() {
    if (p_audio != nullptr) {
      uint8_t buf[kAudioReadChunkSize];
      size_t n = p_audio->readBytes(buf, sizeof(buf));
      if (n > 0) {
        LockGuard guard(p_mutex);
        p_muxer->setStreamType(StreamContentType::Audio);
        if (p_audio_encoder != nullptr) {
          p_audio_encoder->write(buf, n);
        } else {
          p_muxer->addAudioFrame(buf, n);
        }
      } else {
        delay(1);
      }
    }
  }

  /// Pulls one frame, encodes/writes it, then sleeps only the remainder
  /// of video_interval_ms - compensating for time spent above (a slow
  /// encoder otherwise makes the real capture cadence slower than the
  /// fps written into the Muxer's video track).
  void videoTaskLoop() {
    uint32_t start = millis();
    if (p_video != nullptr && p_muxer != nullptr) {
      size_t len = 0;
      const uint8_t *data = p_video->nextFrame(len);
      if (data != nullptr && len > 0) {
        LockGuard guard(p_mutex);
        p_muxer->setStreamType(StreamContentType::Video);
        if (p_video_encoder != nullptr) {
          p_video_encoder->write(data, len);
        } else {
          bool is_key = p_muxer->getVideoInfo().format == VideoFormat::H264
                            ? isH264KeyFrame(data, len)
                            : true;
          p_muxer->addVideoFrame(data, len, is_key);
        }
      }
    }
    uint32_t elapsed = millis() - start;
    if (elapsed < video_interval_ms) delay(video_interval_ms - elapsed);
  }
};

}  // namespace audio_tools
