#pragma once

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/AudioCodecs/AudioEncoded.h"
#include "AudioTools/AudioCodecs/ContainerCommon.h"
#include "AudioTools/AudioCodecs/MultiDecoder.h"
#include "AudioTools/CoreAudio/AudioIO.h"
#include "AudioTools/Video/MultiVideoDecoder.h"
#include "AudioTools/Video/MultiVideoDemuxer.h"
#include "AudioTools/Video/Video.h"

namespace audio_tools {

/**
 * @brief High-level video playback pipeline and controller - the video
 * counterpart of AudioPlayer (CoreAudio/AudioPlayer.h). Wraps a container
 * Demuxer, a VideoDecoder driven through a PacedVideoOutput, and
 * optionally an audio decode chain synced against it, behind one object
 * driven by a single copy() call per loop() iteration.
 *
 * Nothing is pre-registered (no container/codec library dependency by
 * default) - register what your content needs:
 * @code
 * DemuxerAVI aviDemuxer;
 * VideoPlayer player(aviDemuxer, tftOutput, audioOut);
 * player.addVideoDecoder(h264Decoder);
 * player.addAudioDecoder(mp3Decoder, "audio/mpeg");
 * player.begin(file);
 * // loop(): if (player.copy() == 0) { file.close(); ... }
 * @endcode
 * Use VideoPlayerFull (VideoPlayerFull.h) instead to pre-register every
 * common container/video/audio codec at once, at the cost of pulling in
 * all of their libraries unconditionally.
 *
 * Pipeline: Stream -> copy() -> Demuxer -> [audio: EncodedAudioStream ->
 * audio output] / [video: PacedVideoOutput -> VideoDecoder -> video
 * output, own background task].
 *
 * See the wiki's [Video Playback](
 * https://github.com/pschatzmann/arduino-audio-tools/wiki/Video-Playback)
 * page for the full picture: audio-clock scheduling
 * (setUseAudioClock()), running video decode on its own core
 * (setTaskParameters()), the seek-backed/spooled MP4 exception, and more.
 *
 * @ingroup player
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VideoPlayer {
 public:
  VideoPlayer() = default;

  /**
   * Video-only playback (no audio track) - auto-detected video codec.
   * `demuxer` is registered the same way an addDemuxer() call would (this
   * is just a convenience for the common single-format case - call
   * addDemuxer() separately, as many times as needed, for additional
   * container formats).
   */
  VideoPlayer(Demuxer& demuxer, VideoOutput& videoOutput) {
    addDemuxer(demuxer);
    setVideoOutput(videoOutput);
  }

  /**
   * Video + audio playback (AudioOutput target, e.g. AudioBoardStream/
   * I2SStream) with auto-detected codecs. By default the video frame
   * schedule is driven off this audio output's own playback progress (an
   * "audio clock") rather than a free-running timer, so video stays in
   * sync with audio even as the two decode at slightly different rates;
   * call setUseAudioClock(false) if the content's audio track never
   * actually delivers bytes (e.g. a silent/empty track), since the clock
   * would then never advance and video would never play. `demuxer` is
   * registered the same way as the video-only constructor above.
   */
  VideoPlayer(Demuxer& demuxer, VideoOutput& videoOutput,
              AudioOutput& audioOutput) {
    addDemuxer(demuxer);
    setVideoOutput(videoOutput);
    setAudioOutput(audioOutput);
  }

  /**
   * Video + audio playback (a generic Print target) with auto-detected
   * codecs - same audio-clock-driven scheduling as the AudioOutput
   * overload above, and `demuxer` is registered the same way as the
   * video-only constructor.
   */
  VideoPlayer(Demuxer& demuxer, VideoOutput& videoOutput,
              Print& audioOutput) {
    addDemuxer(demuxer);
    setVideoOutput(videoOutput);
    setAudioOutput(audioOutput);
  }

  /**
   * Video + audio playback (AudioStream target, e.g. PortAudioStream) with
   * auto-detected codecs - same audio-clock-driven scheduling as the
   * AudioOutput overload above, and `demuxer` is registered the same way
   * as the video-only constructor.
   */
  VideoPlayer(Demuxer& demuxer, VideoOutput& videoOutput,
              AudioStream& audioOutput) {
    addDemuxer(demuxer);
    setVideoOutput(videoOutput);
    setAudioOutput(audioOutput);
  }

  /**
   * Non-copyable: video_sync/audio_out below are wired against this
   * object's own member addresses - copying would leave the copy's
   * internal wiring pointing at the original.
   */
  VideoPlayer(VideoPlayer const&) = delete;
  VideoPlayer& operator=(VideoPlayer const&) = delete;

  /**
   * Registers a container demuxer implementation (DemuxerAVI/DemuxerMP4/
   * DemuxerMPG/...) with the built-in MultiVideoDemuxer, keyed under the
   * demuxer's own Demuxer::mimeVideo(). The built-in demuxer selector
   * starts out completely empty, so this needs at least one call per
   * container format your content actually uses before begin() is called
   * - call it multiple times to support more than one format.
   */
  void addDemuxer(Demuxer& demuxer) { default_demuxer.addDemuxer(demuxer); }

  /**
   * Registers a video codec decoder (H264Decoder/MJPEGDecoder/MPGDecoder/
   * ...) with the built-in MultiVideoDecoder, keyed under the decoder's
   * own VideoDecoder::codecFormat(). The built-in decoder selector starts
   * out completely empty, so this needs at least one call per video codec
   * your content actually uses before begin() is called - call it
   * multiple times to support more than one codec.
   */
  void addVideoDecoder(VideoDecoder& decoder) {
    default_video_decoder.addDecoder(decoder);
  }

  /**
   * Defines the final video display target (e.g. OutputTinyGPU,
   * OutputOpenCV) - the caller must already have configured/begin()'d it
   * (board/panel init, such as pin setup or a TFT library's own begin(),
   * is outside this class's scope). Call before begin().
   */
  void setVideoOutput(VideoOutput& out) { p_video_output = &out; }

  /**
   * Registers an audio codec decoder (MP3DecoderHelix/AACDecoderHelix/...)
   * with the built-in MultiDecoder, keyed by the `mime` string given here
   * (or the decoder's own default mime type if `mime` is null). The
   * built-in decoder selector starts out completely empty, so this needs
   * at least one call per audio codec your content actually uses before
   * begin() is called - call it multiple times to support more than one
   * codec.
   */
  void addAudioDecoder(AudioDecoder& decoder, const char* mime = nullptr) {
    default_audio_decoder.addDecoder(decoder, mime);
  }

  /**
   * Defines the final audio output target and enables the audio playback
   * path - leave unset entirely for video-only content. The caller must
   * already have configured/begin()'d it. Call before begin(). Also
   * enables audio-clock-driven video scheduling by default (see
   * setUseAudioClock()) - call setUseAudioClock(false) afterwards to turn
   * that off.
   *
   * Three overloads, mirroring AudioPlayer::setOutput(): AudioOutput and
   * AudioStream are unrelated types (both derive from Print, but neither
   * derives from the other), so the most specific one actually available
   * should be used. Using the specific overload lets begin() wire the
   * audio clock/EncodedAudioStream through the matching
   * setOutput(AudioOutput&)/setStream(AudioStream&) overload instead of
   * falling back to the generic Print& one - that's what makes things
   * like audio-info change notifications reach the real output correctly.
   */
  void setAudioOutput(Print& out) {
    p_audio_output = &out;
    p_audio_output_typed = nullptr;
    p_audio_stream = nullptr;
  }
  void setAudioOutput(AudioOutput& out) {
    p_audio_output = &out;
    p_audio_output_typed = &out;
    p_audio_stream = nullptr;
  }
  void setAudioOutput(AudioStream& out) {
    p_audio_output = &out;
    p_audio_output_typed = nullptr;
    p_audio_stream = &out;
  }

  /**
   * Whether video frames are scheduled against the real audio output's
   * playback progress (an "audio clock", via AudioTimeSourceStream)
   * instead of a free-running timer - keeps video in sync with audio even
   * as the two decode at slightly different rates, since the audio clock
   * only advances as audio is actually consumed. True (on) by default
   * once an audio output is set via setAudioOutput() - calling
   * setAudioOutput() again does not reset this. Turn it off if the
   * content's audio track never actually delivers bytes (e.g. a
   * silent/empty track), since the clock would then never advance and
   * video would never play. Call before begin().
   */
  void setUseAudioClock(bool active) { use_audio_clock = active; }
  /// Current setUseAudioClock() setting.
  bool useAudioClock() const { return use_audio_clock; }

  /**
   * Sets the read-buffer size (bytes) copy() uses per call (default 1024,
   * same as CodecCopy's own default). Call before begin().
   */
  void setBufferSize(int size) { buffer_size = size; }

  /**
   * Corrects for the real audio output's own buffering latency: the audio
   * clock advances as soon as bytes are accepted by write(), not when
   * they actually become audible - e.g. ~100ms ahead on a device with
   * ~100ms of internal buffering, which would otherwise make every video
   * frame appear that much too early. Delaying each frame's schedule by
   * this amount cancels that out. There's no way to derive it
   * automatically - set it to roughly match the audio output's own
   * buffering latency. 0 (the default) applies no correction. Forwards to
   * the built-in PacedVideoOutput; call before begin().
   */
  void setSchedulingDelayMs(uint32_t delayMs) {
    video_sync.setSchedulingDelayMs(delayMs);
  }
  /**
   * How many frame periods behind schedule playback must fall before
   * non-keyframes start being proactively dropped to catch up, instead of
   * only dropping once the frame queue is completely full. 1.0 (the
   * default) starts dropping once the most recently rendered frame was at
   * least one frame period late; a lower value catches up faster at the
   * cost of more dropped frames, a higher value tolerates more backlog
   * before reacting. Forwards to the built-in PacedVideoOutput; call
   * before begin().
   */
  void setCatchUpThresholdFrames(float frames) {
    video_sync.setCatchUpThresholdFrames(frames);
  }
  /**
   * Unconditionally discards every non-keyframe (P/B-frame) as it
   * arrives, before it is even queued for decode - a codec-agnostic way
   * to render only keyframes, cheaper than letting the decoder decode and
   * then discard each one. Off by default. Independent of the
   * catch-up/backlog-driven dropping described above, which still applies
   * when this is off. Forwards to the built-in PacedVideoOutput; call
   * before begin().
   */
  void setIgnorePFrames(bool active) { video_sync.setIgnorePFrames(active); }
  /**
   * How far behind schedule (milliseconds) playback must fall before it
   * gives up trying to catch up frame-by-frame and instead jumps its
   * internal schedule forward to the current time - dropping frames alone
   * can only slow a backlog's growth, never actually shrink it, since
   * decoding can't outrun real time. Content in the skipped gap is never
   * shown - this trades a visible jump for recovering instead of lagging
   * further and further behind. 2000ms by default; 0 disables this
   * resync behavior entirely. Forwards to the built-in PacedVideoOutput;
   * call before begin().
   */
  void setResyncThresholdMs(uint32_t ms) {
    video_sync.setResyncThresholdMs(ms);
  }
  /**
   * Byte-occupancy fraction (0..1) of the internal frame queue that
   * triggers the same forward-jump resync as setResyncThresholdMs(), but
   * from a different signal: dropped frames can keep the most recently
   * rendered frame's own lateness low even while newer, not-yet-rendered
   * bytes keep piling up faster than they're consumed. 0.8 (80% full) by
   * default; 0 disables this trigger. Forwards to the built-in
   * PacedVideoOutput; call before begin().
   */
  void setResyncQueueFillFraction(float fraction) {
    video_sync.setResyncQueueFillFraction(fraction);
  }
  /**
   * Number of not-yet-consumed keyframes sitting in the queue that
   * triggers the same resync as the other two thresholds - catches a
   * growing backlog earlier than either: once several groups-of-pictures
   * worth of unconsumed keyframes have piled up, none of them are worth
   * rendering in order anymore, so the freshest one is kept and the rest
   * discarded. 3 by default (deliberately small); 0 disables this
   * trigger. Forwards to the built-in PacedVideoOutput; call before
   * begin().
   */
  void setMaxQueuedIFrames(int count) {
    video_sync.setMaxQueuedIFrames(count);
  }
  /**
   * Stack size (in words), priority, and (optionally) CPU core for the
   * background task that renders queued video frames on schedule. `core`
   * pins that task to a specific core - e.g. core 0, to keep it off
   * whichever core runs copy()/loop() (audio decode/demuxing), giving
   * audio and video decoding their own separate cores with no further
   * setup. -1 (the default) leaves core assignment up to the RTOS.
   * Forwards to the built-in PacedVideoOutput; call before begin() - has
   * no effect on an already-running render task.
   */
  void setTaskParameters(uint32_t stackSizeWords, uint8_t priority,
                          int core = -1) {
    video_sync.setTaskParameters(stackSizeWords, priority, core);
  }
  /**
   * Byte capacity of the frame queue sitting between the demuxer/decoder
   * and the paced render task - default 32KB. Too small behaves like a
   * 1-frame queue, where any single slow frame blocks the writer
   * immediately; a larger queue absorbs transient jitter at the cost of
   * more RAM and more frames buffered ahead of what's actually on screen.
   * It does not change what happens under a sustained rate mismatch - the
   * queue still eventually fills, just later. Forwards to the built-in
   * PacedVideoOutput; call before begin()/the first frame - not supported
   * afterwards.
   */
  void setQueueBytes(size_t bytes) { video_sync.setQueueBytes(bytes); }
  /**
   * Whether the frame queue above is allocated from PSRAM instead of the
   * internal heap - silently falls back to internal heap on boards
   * without PSRAM. On by default, since internal heap is a scarcer shared
   * resource on most ESP32 boards and video decoding already needs PSRAM
   * for other buffers (decoded picture buffers, scaling scratch space,
   * ...). Turn it off for a queue small enough that internal heap is
   * preferable, or on a board with no PSRAM where the fallback path is
   * undesired. Forwards to the built-in PacedVideoOutput; call before
   * begin()/the first frame - has no effect on an already-allocated
   * queue.
   */
  void setQueueUsePSRAM(bool flag) { video_sync.setQueueUsePSRAM(flag); }

  /**
   * Wires the full pipeline (video decoder -> output via PacedVideoOutput;
   * audio decoder -> [audio clock ->] output, if an audio output was
   * given) and starts the demuxer. `source` (e.g. an open File) is read
   * from on every subsequent copy() call - the caller owns it and is
   * responsible for closing it once copy()/copyAll() signal end of stream
   * (return 0).
   */
  bool begin(Stream& source) {
    if (p_video_output == nullptr) {
      LOGE("VideoPlayer: no VideoOutput set - call setVideoOutput() first");
      return false;
    }
    p_source = &source;

    // video: decoder -> display, buffered/paced through video_sync
    // (video_sync's target is fixed at construction time - it is
    // initialized from default_video_decoder in the member declaration
    // below)
    default_video_decoder.setOutput(*p_video_output);
    default_video_decoder.setVideoInfoSource(default_demuxer);
    // Default no-op for a VideoOutput that doesn't need it (see
    // VideoOutput::setVideoInfoSource()'s own comment) - saves every
    // sketch its own setVideoInfoSource() call for one that does (e.g.
    // OutputTinyGPU/OutputOpenCV/OutputTFT_eSPI).
    p_video_output->setVideoInfoSource(default_demuxer);
    if (!default_video_decoder.begin()) {
      LOGE("VideoPlayer: MultiVideoDecoder begin() failed");
      return false;
    }
    default_demuxer.setOutputVideo(video_sync);

    // audio path is optional - only wired up when setAudioOutput() was
    // called
    if (p_audio_output != nullptr) {
      default_audio_decoder.setMimeSource(default_demuxer);
      audio_out.setDecoder(&default_audio_decoder);
      if (use_audio_clock) {
        // Route through the most specific overload available (see
        // setAudioOutput()'s own comment) so audio-info change
        // notifications still reach the real output.
        if (p_audio_output_typed != nullptr) {
          audio_clock.setOutput(*p_audio_output_typed);
        } else if (p_audio_stream != nullptr) {
          audio_clock.setStream(*p_audio_stream);
        } else {
          audio_clock.setOutput(*p_audio_output);
        }
        audio_out.setStream(audio_clock);
        video_sync.setAudioClock(audio_clock);
      } else if (p_audio_output_typed != nullptr) {
        audio_out.setOutput(*p_audio_output_typed);
      } else if (p_audio_stream != nullptr) {
        audio_out.setStream(*p_audio_stream);
      } else {
        audio_out.setOutput(*p_audio_output);
      }
      if (!audio_out.begin()) {
        LOGE("VideoPlayer: EncodedAudioStream begin() failed");
        return false;
      }
      default_demuxer.setOutputAudio(audio_out);
    }

    if (!default_demuxer.begin()) {
      LOGE("VideoPlayer: MultiVideoDemuxer begin() failed");
      return false;
    }
    active = true;
    return true;
  }

  /**
   * Stops playback and releases the video/audio decoders + the sync
   * task's background render thread. The Stream given to begin() is left
   * for the caller to close.
   */
  void end() {
    active = false;
    video_sync.end();
    default_video_decoder.end();
    if (p_audio_output != nullptr) audio_out.end();
    default_demuxer.end();
  }

  /**
   * Reads and demuxes one chunk (setBufferSize(), default 1024 bytes)
   * from the source given to begin() - call this every loop() iteration.
   * Also keeps the video schedule's fps in sync with the demuxer's own
   * parsed rate, since VideoInfo::fps only becomes known partway through
   * (once the container's header has been parsed) - no separate polling
   * needed in the caller's loop(), unlike the *.ino examples this class
   * replaces.
   * @return bytes actually copied - 0 once the source is exhausted (or
   * playback is inactive/not begin()'d), matching CodecCopy::copy()'s own
   * "0 means stop" convention every current *.ino example already checks
   * for.
   */
  size_t copy() {
    if (!active || p_source == nullptr) return 0;
    float fps = default_demuxer.getVideoInfo().fps;
    if (fps > 0) video_sync.setFps(fps);
    uint8_t buffer[buffer_size];
    size_t len = p_source->readBytes(buffer, buffer_size);
    if (len == 0) return 0;
    // Retry on a partial accept instead of dropping the remainder, which
    // would desync the demuxer from the real byte stream (duplicated here
    // rather than reusing CodecCopy::writeAll() since CodecCopy binds its
    // Stream& at construction time, before begin(Stream&)'s source is
    // known).
    size_t written = 0;
    while (written < len) {
      size_t n = default_demuxer.write(buffer + written, len - written);
      if (n == 0) break;
      written += n;
    }
    return written;
  }

  /// Copies until the source is exhausted (blocking).
  size_t copyAll() {
    size_t result = 0;
    size_t step = copy();
    while (step > 0) {
      result += step;
      step = copy();
    }
    return result;
  }

  /// True between a successful begin() and end()/setActive(false)/source
  /// exhaustion.
  bool isActive() const { return active; }
  /// Same as isActive().
  operator bool() const { return active; }

  /**
   * Halts copy()/copyAll() without tearing down the pipeline (unlike
   * end()) - resume with setActive(true).
   */
  void setActive(bool isActive) { active = isActive; }

  /**
   * The built-in demuxer multi-selector, empty until addDemuxer() has
   * registered at least one container demuxer - also useful for
   * diagnostics (e.g. selectedDemuxer()/mimeVideo()).
   */
  MultiVideoDemuxer& demuxer() { return default_demuxer; }
  /**
   * The built-in video multi-decoder, empty until addVideoDecoder() has
   * registered at least one video codec decoder - also useful for
   * diagnostics (e.g. selectedFormat()).
   */
  MultiVideoDecoder& videoDecoder() { return default_video_decoder; }
  /**
   * The built-in audio multi-decoder, empty until addAudioDecoder() has
   * registered at least one audio codec decoder - also useful for
   * diagnostics (e.g. selectedMime()).
   */
  MultiDecoder& audioDecoder() { return default_audio_decoder; }
  /**
   * Escape hatch for tuning beyond this class's own surface - e.g.
   * setTaskParameters()/setQueueBytes()/setMaxQueuedIFrames()/
   * setIgnorePFrames()/setSchedulingDelayMs(), or the frameCount()/
   * avgFrameMs()/outputFPS() family of diagnostics - returns the
   * PacedVideoOutput instance that VideoPlayer's own tuning forwarders
   * above delegate to.
   */
  PacedVideoOutput& videoSyncTask() { return video_sync; }
  /// The Stream given to begin() - nullptr before the first begin() call.
  Stream* getStream() { return p_source; }
  /**
   * Logs periodic playback diagnostics (frame count, average frame time,
   * output fps, ...) to `out` - convenience shortcut for
   * videoSyncTask().logTo(out).
   */
  void logTo(Print& out) { video_sync.logTo(out); }

 protected:
  VideoOutput* p_video_output = nullptr;
  Print* p_audio_output = nullptr;
  // Set alongside p_audio_output by the matching setAudioOutput()
  // overload - see its own comment for why begin() prefers these over
  // the generic Print* above when wiring audio_clock/audio_out.
  AudioOutput* p_audio_output_typed = nullptr;
  AudioStream* p_audio_stream = nullptr;
  Stream* p_source = nullptr;

  // Empty until addDemuxer() registers a container demuxer.
  MultiVideoDemuxer default_demuxer;
  // Declared before video_sync below, which takes this address at
  // construction time.
  MultiVideoDecoder default_video_decoder;
  // Empty until addAudioDecoder() registers an audio codec decoder.
  MultiDecoder default_audio_decoder;

  PacedVideoOutput video_sync{default_video_decoder};
  AudioTimeSourceStream audio_clock;
  EncodedAudioStream audio_out;

  bool use_audio_clock = true;
  int buffer_size = 1024;
  bool active = false;
};

}  // namespace audio_tools
