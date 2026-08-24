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
 * counterpart of AudioPlayer (CoreAudio/AudioPlayer.h). Wraps every class
 * a video playback example otherwise wires by hand (a container Demuxer,
 * a VideoDecoder driven through a PacedVideoOutput, and optionally an
 * audio decode chain synced against it) behind one object driven by a
 * single copy() call per loop() iteration.
 *
 * Demuxes the container through a built-in MultiVideoDemuxer, decodes
 * video through a built-in MultiVideoDecoder, and decodes audio through a
 * built-in MultiDecoder - all three start with NOTHING registered (see
 * each one's own class comment) and none of them pulls in any
 * container/codec library on its own, so including this header alone
 * adds no external dependency at all. Register whatever your content
 * actually needs via addDemuxer()/addVideoDecoder()/addAudioDecoder(),
 * exactly as MultiVideoDemuxerFull's/MultiVideoDecoderFull's/
 * DecoderHelix's own constructors do internally:
 * @code
 * DemuxerAVI aviDemuxer;
 * VideoPlayer player(aviDemuxer, tftOutput, audioOut);
 * H264Decoder h264Decoder;
 * MJPEGDecoder mjpegDecoder;
 * MP3DecoderHelix mp3Decoder;
 * AACDecoderHelix aacDecoder;
 * void setup() {
 *   ...
 *   player.addVideoDecoder(h264Decoder);
 *   player.addVideoDecoder(mjpegDecoder);
 *   player.addAudioDecoder(mp3Decoder, "audio/mpeg");
 *   player.addAudioDecoder(aacDecoder, "audio/aac");
 *   player.begin(file);
 * }
 * void loop() {
 *   if (player.copy() == 0) { file.close(); ... }
 * }
 * @endcode
 * matching sd-avi-mjpg-video.ino/decode-avi.ino's own pipeline - `demuxer`
 * is just a convenience for this common single-format case (it's
 * registered exactly the way an addDemuxer() call would be); call
 * addDemuxer() separately, as many times as needed, for additional
 * container formats. Use VideoPlayerFull (VideoPlayerFull.h)
 * instead for the "just point it at a file, any common format just works"
 * convenience with no registration code of your own needed - it
 * pre-registers every container format (AVI/MP4/MPG), every video codec
 * (H264/MJPEG/MPEG-1), and the most common audio ones (MP3/AAC/MP2), at
 * the cost of pulling in every one of their parser/codec libraries
 * unconditionally (see its own class comment).
 *
 * addAudioDecoder() also covers a codec the DemuxerXxx::mime() the
 * selected demuxer reports precisely but a plain byte-sniffing
 * MimeDetector wouldn't otherwise recognize, e.g. MP2Decoder (TinyMP2)
 * for MPEG-1 Layer II audio, the same way sd-mpg-video.ino's hand-wired
 * pipeline does:
 * @code
 * player.addAudioDecoder(mp2Decoder, "audio/mpeg; codecs=\"mpeg1-layer2\"");
 * @endcode
 * addVideoDecoder() similarly covers a hardware-accelerated decoder like
 * H264DecoderESP32S3 (CodecH264ESP32S3.h) in place of a portable one -
 * note its VideoDecoder::isValid() isn't overridden (see the class
 * comment on that method), so it only gets selected via a
 * setVideoInfoSource() answer, not the content-sniffing fallback; set one
 * (typically the demuxer feeding this player) if you rely on it. There is
 * no way to replace any of the three built-in multi-selectors wholesale
 * (e.g. with a single-format demuxer/decoder) - add to what's already
 * registered instead. A video-only stream needs no audio decoder
 * registered at all - see setAudioOutput()/the class's video-only
 * constructor.
 *
 * Pipeline: Stream (source) -> copy() (CodecCopy-equivalent) -> Demuxer
 *   (demux)
 *   -> EncodedAudioStream (AudioDecoder) -> [AudioTimeSourceStream (audio
 *   clock), see setUseAudioClock()] -> audio output
 *   \-> PacedVideoOutput (buffer + schedule) -> VideoDecoder -> video
 *   output (own background task)
 *
 * Audio clock: scheduling video against real playback progress
 * (PacedVideoOutput::setAudioClock()) needs an audio clock that's
 * actually advancing - wiring one against a track that never delivers any
 * bytes (silent/absent audio) would stall video forever waiting for a
 * clock that never moves (see decode-mp4.ino's own history for exactly
 * this bug). setUseAudioClock() therefore defaults to false: video is
 * scheduled against wall-clock time unless you explicitly opt in once an
 * audio output is wired AND you know the content actually has a real
 * audio track.
 *
 * Only covers the common "feed a Demuxer straight from a Stream" case,
 * matching the RAM-backed sample-table default every Demuxer uses out of
 * the box - the seek-backed/spooled MP4 strategies (see
 * decode-mp4-file.ino/decode-mp4-spooled.ino) feed their Demuxer through
 * their own FileSeekableSource/SpoolStorageFactory machinery instead, for
 * their own memory-optimization reasons, and are not wrapped here; keep
 * your own reference to the concrete DemuxerMP4 you construct and pass to
 * addDemuxer() (it stays fully usable on its own - MultiVideoDemuxer just
 * forwards write()/setOutputAudio()/setOutputVideo()/... to it once
 * selected) and drive it directly instead of calling copy()/copyAll() on
 * this class.
 *
 * Operation model: call copy() regularly (non-blocking) in loop(), or
 * copyAll() for blocking end-to-end playback.
 *
 * Dependencies: none - this header itself doesn't pull in any
 * container/codec library (see above). Bring whichever demuxers and
 * video/audio codec libraries your content actually needs and register
 * them via addDemuxer()/addVideoDecoder()/addAudioDecoder(), or use
 * VideoPlayerFull for all of them at once.
 *
 * @ingroup player
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VideoPlayer {
 public:
  VideoPlayer() = default;

  /// Video-only playback (no audio track) - auto-detected video codec.
  /// `demuxer` is registered the same way an addDemuxer() call would
  /// (this is just a convenience for the common single-format case - call
  /// addDemuxer() separately, as many times as needed, for additional
  /// container formats).
  VideoPlayer(Demuxer& demuxer, VideoOutput& videoOutput) {
    addDemuxer(demuxer);
    setVideoOutput(videoOutput);
  }

  /// Video + audio playback (AudioOutput target, e.g. AudioBoardStream/
  /// I2SStream) with auto-detected codecs. See the class comment's "Audio
  /// clock" section - setUseAudioClock(true) still needs to be called
  /// explicitly if the content actually has a real audio track and should
  /// be scheduled against it. See the video-only constructor above for
  /// `demuxer`.
  VideoPlayer(Demuxer& demuxer, VideoOutput& videoOutput, AudioOutput& audioOutput) {
    addDemuxer(demuxer);
    setVideoOutput(videoOutput);
    setAudioOutput(audioOutput);
  }

  /// Video + audio playback (a generic Print target) with auto-detected
  /// codecs. See the class comment's "Audio clock" section, and the
  /// video-only constructor above for `demuxer`.
  VideoPlayer(Demuxer& demuxer, VideoOutput& videoOutput, Print& audioOutput) {
    addDemuxer(demuxer);
    setVideoOutput(videoOutput);
    setAudioOutput(audioOutput);
  }

  /// Video + audio playback (AudioStream target, e.g. PortAudioStream)
  /// with auto-detected codecs. See the class comment's "Audio clock"
  /// section, and the video-only constructor above for `demuxer`.
  VideoPlayer(Demuxer& demuxer, VideoOutput& videoOutput, AudioStream& audioOutput) {
    addDemuxer(demuxer);
    setVideoOutput(videoOutput);
    setAudioOutput(audioOutput);
  }

  /// Non-copyable: video_sync/audio_out below are wired against this
  /// object's own member addresses - copying would leave the copy's
  /// internal wiring pointing at the original.
  VideoPlayer(VideoPlayer const&) = delete;
  VideoPlayer& operator=(VideoPlayer const&) = delete;

  /// Registers a container demuxer (DemuxerAVI/DemuxerMP4/DemuxerMPG/...)
  /// with the built-in (initially empty) MultiVideoDemuxer, under its own
  /// Demuxer::mimeVideo() - see MultiVideoDemuxer::addDemuxer(). Nothing
  /// is registered by default (see the class comment), so this needs at
  /// least one call per container format your content actually uses.
  /// Call before begin().
  void addDemuxer(Demuxer& demuxer) { default_demuxer.addDemuxer(demuxer); }

  /// Registers a video codec with the built-in (initially empty)
  /// MultiVideoDecoder, under its own VideoDecoder::codecFormat() - see
  /// MultiVideoDecoder::addDecoder(). Nothing is registered by default
  /// (see the class comment), so this needs at least one call per codec
  /// your content's video track actually uses. Call before begin().
  void addVideoDecoder(VideoDecoder& decoder) {
    default_video_decoder.addDecoder(decoder);
  }

  /// Defines the final video display target (e.g. OutputTinyGPU,
  /// OutputOpenCV) - must already be configured/begin()'d (board/panel
  /// init is outside this class's scope, same as AudioPlayer's AudioOutput&
  /// argument). Call before begin().
  void setVideoOutput(VideoOutput& out) { p_video_output = &out; }

  /// Registers an audio codec with the built-in (initially empty)
  /// MultiDecoder - see MultiDecoder::addDecoder() (the mime-only
  /// overload). Nothing is registered by default (see the class
  /// comment), so this needs at least one call per codec your content's
  /// audio track actually uses. Call before begin().
  void addAudioDecoder(AudioDecoder& decoder, const char* mime = nullptr) {
    default_audio_decoder.addDecoder(decoder, mime);
  }

  /// Defines the final audio output target and enables the audio path -
  /// leave unset entirely for video-only content. Must already be
  /// configured/begin()'d. Call before begin(). See the class comment's
  /// "Audio clock" section for setUseAudioClock()'s default.
  ///
  /// Three overloads, mirroring AudioPlayer::setOutput() - AudioOutput and
  /// AudioStream are unrelated types (both derive from Print, neither from
  /// the other - see AudioOutput.h's own class comment), so the most
  /// specific one actually available should be used: it lets begin() wire
  /// the audio clock/EncodedAudioStream via the matching setOutput(
  /// AudioOutput&)/setStream(AudioStream&) overload instead of falling
  /// back to the generic Print& one, which is what makes e.g. audio-info
  /// change notifications reach the real output.
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

  /// See the class comment's "Audio clock" section. Off by default;
  /// setAudioOutput() does not change it - opt in explicitly once you
  /// know the content has a real audio track. Call before begin().
  void setUseAudioClock(bool active) { use_audio_clock = active; }
  bool useAudioClock() const { return use_audio_clock; }

  /// Sets the read-buffer size (bytes) copy() uses per call (default
  /// 1024, same as CodecCopy's own default). Call before begin().
  void setBufferSize(int size) { buffer_size = size; }

  /// Wires the full pipeline (video decoder -> output via
  /// PacedVideoOutput; audio decoder -> [audio clock ->] output, if an
  /// audio output was given) and starts the demuxer. `source` (e.g. an
  /// open File) is read from on every subsequent copy() call - the caller
  /// owns it and is responsible for closing it once copy()/copyAll()
  /// signal end of stream (return 0).
  bool begin(Stream& source) {
    if (p_video_output == nullptr) {
      LOGE("VideoPlayer: no VideoOutput set - call setVideoOutput() first");
      return false;
    }
    p_source = &source;

    // video: decoder -> display, buffered/paced through video_sync
    // (video_sync's target is fixed at construction - see its own member
    // declaration below)
    default_video_decoder.setOutput(*p_video_output);
    default_video_decoder.setVideoInfoSource(default_demuxer);
    if (!default_video_decoder.begin()) {
      LOGE("VideoPlayer: MultiVideoDecoder begin() failed");
      return false;
    }
    default_demuxer.setOutputVideo(video_sync);

    // audio (optional - see the class comment's "Audio clock" section)
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

  /// Stops playback and releases the video/audio decoders + the sync
  /// task's background render thread. The Stream given to begin() is left
  /// for the caller to close.
  void end() {
    active = false;
    video_sync.end();
    default_video_decoder.end();
    if (p_audio_output != nullptr) audio_out.end();
    default_demuxer.end();
  }

  /// Reads and demuxes one chunk (setBufferSize(), default 1024 bytes)
  /// from the source given to begin() - call this every loop() iteration.
  /// Also keeps the video schedule's fps in sync with the demuxer's own
  /// parsed rate, since VideoInfo::fps only becomes known partway through
  /// (once the container's header has been parsed) - no separate polling
  /// needed in the caller's loop(), unlike the *.ino examples this class
  /// replaces.
  /// @return bytes actually copied - 0 once the source is exhausted (or
  /// playback is inactive/not begin()'d), matching CodecCopy::copy()'s own
  /// "0 means stop" convention every current *.ino example already checks
  /// for.
  size_t copy() {
    if (!active || p_source == nullptr) return 0;
    float fps = default_demuxer.getVideoInfo().fps;
    if (fps > 0) video_sync.setFps(fps);
    uint8_t buffer[buffer_size];
    size_t len = p_source->readBytes(buffer, buffer_size);
    if (len == 0) return 0;
    // Retry on a partial accept instead of dropping the remainder, which
    // would desync the demuxer from the real byte stream (see
    // CodecCopy::writeAll()'s own comment - duplicated here rather than
    // reused since CodecCopy binds its Stream& at construction time,
    // before begin(Stream&)'s source is known).
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
  operator bool() const { return active; }

  /// Halts copy()/copyAll() without tearing down the pipeline (unlike
  /// end()) - resume with setActive(true).
  void setActive(bool isActive) { active = isActive; }

  /// The built-in demuxer multi-selector (empty until addDemuxer() is
  /// called - see the class comment) - also useful for diagnostics (e.g.
  /// selectedDemuxer()/mimeVideo()).
  MultiVideoDemuxer& demuxer() { return default_demuxer; }
  /// The built-in video multi-decoder (empty until addVideoDecoder() is
  /// called - see the class comment) - also useful for diagnostics (e.g.
  /// selectedFormat()).
  MultiVideoDecoder& videoDecoder() { return default_video_decoder; }
  /// The built-in audio multi-decoder (empty until addAudioDecoder() is
  /// called - see the class comment) - also useful for diagnostics (e.g.
  /// selectedMime()).
  MultiDecoder& audioDecoder() { return default_audio_decoder; }
  /// Escape hatch for tuning beyond this class's own surface - e.g.
  /// setTaskParameters()/setQueueBytes()/setMaxQueuedIFrames()/
  /// setIgnorePFrames()/setSchedulingDelayMs(), or the frameCount()/
  /// avgFrameMs()/outputFPS() family of diagnostics.
  PacedVideoOutput& videoSyncTask() { return video_sync; }
  /// The Stream given to begin() - nullptr before the first begin() call.
  Stream* getStream() { return p_source; }

 protected:
  VideoOutput* p_video_output = nullptr;
  Print* p_audio_output = nullptr;
  // Set alongside p_audio_output by the matching setAudioOutput()
  // overload - see its own comment for why begin() prefers these over
  // the generic Print* above when wiring audio_clock/audio_out.
  AudioOutput* p_audio_output_typed = nullptr;
  AudioStream* p_audio_stream = nullptr;
  Stream* p_source = nullptr;

  // Empty by default (see the class comment) - populated via
  // addDemuxer().
  MultiVideoDemuxer default_demuxer;
  // Declared before video_sync below, which takes this address at
  // construction time.
  MultiVideoDecoder default_video_decoder;
  // Empty by default (see the class comment) - populated via
  // addAudioDecoder().
  MultiDecoder default_audio_decoder;

  PacedVideoOutput video_sync{default_video_decoder};
  AudioTimeSourceStream audio_clock;
  EncodedAudioStream audio_out;

  bool use_audio_clock = false;
  int buffer_size = 1024;
  bool active = false;
};

}  // namespace audio_tools
