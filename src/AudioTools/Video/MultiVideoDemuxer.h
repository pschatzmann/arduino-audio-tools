#pragma once

#include "AudioTools/AudioCodecs/ContainerCommon.h"
#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/CoreAudio/AudioBasic/StrView.h"

namespace audio_tools {

/**
 * @brief Manages multiple Demuxers with automatic container-format
 * detection - the container-side counterpart of MultiVideoDecoder
 * (Video/MultiVideoDecoder.h), which does the same job one layer down for
 * the video elementary stream's own codec. No demuxers are registered by
 * default and this header has no container-parser dependency of its own
 * (it only needs the Demuxer interface, ContainerCommon.h) - register
 * whichever concrete demuxers (DemuxerAVI/DemuxerMP4/DemuxerMPG) your
 * content actually needs via addDemuxer().
 *
 * Drop it in wherever a plain Demuxer& is expected - VideoPlayer in fact
 * uses one internally as its own built-in demuxer, with addDemuxer()
 * registering against it - or feed it via write() directly, and it
 * self-selects the right registered demuxer from the stream's own
 * container signature instead of the caller having to know the file
 * format up front.
 *
 * Detection runs once, on the very first write() call: each registered
 * demuxer's own Demuxer::isValid() (content-sniffing - a virtual method
 * with a default-false implementation, so a demuxer only takes part if it
 * actually overrides it; DemuxerAVI/DemuxerMP4/DemuxerMPG all do - see
 * their own class comments) is tried, in registration order, against that
 * first call's bytes. Reliable in practice since every real container
 * signature (AVI's "RIFF"/"AVI ", MP4's "ftyp" box, MPG's pack
 * start_code) falls within the first dozen bytes of the file, comfortably
 * inside any realistic first read/write() chunk. Every subsequent write()
 * call, plus every other Demuxer method, is forwarded to that same
 * demuxer for the rest of the stream - never re-detected.
 *
 * setOutputAudio()/setOutputVideo()/setSendWavHeader() are applied to
 * every registered demuxer eagerly (cheap - just stores pointers/a flag,
 * no allocation) - call addDemuxer() for all of them before these, so
 * whichever one gets selected on the first write() is already wired. Only
 * the actually-selected demuxer's begin() is called (on selection, from
 * inside write()) - matching MultiVideoDecoder's own memory-efficient
 * lazy-init rationale.
 *
 * @ingroup codecs
 * @ingroup decoder
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MultiVideoDemuxer : public Demuxer {
 public:
  /// Registers a demuxer under its own Demuxer::mimeVideo() - see the
  /// class comment for when/how it's later selected (content-sniffing via
  /// its own Demuxer::isValid()). Call before the first write() reaches
  /// this object.
  ///
  /// Replaces, rather than adds to, any demuxer already registered under
  /// the same mimeVideo() - at most one entry per container format, for
  /// the same reason MultiVideoDecoder::addDecoder() does: a stale second
  /// entry for the same format would otherwise be permanently
  /// unreachable dead weight, never actually selected.
  void addDemuxer(Demuxer &demuxer) {
    const char *mime_video = demuxer.mimeVideo();
    for (int i = 0; i < demuxers.size(); i++) {
      if (StrView(demuxers[i]->mimeVideo()).equals(mime_video)) {
        demuxers.erase(i);
        break;
      }
    }
    demuxer.addNotifyAudioChange(*this);
    demuxers.push_back(&demuxer);
  }

  /// Defines the audio output stream - forwarded to every registered
  /// demuxer eagerly (see class comment).
  void setOutputAudio(Print &out) override {
    for (int i = 0; i < demuxers.size(); i++) demuxers[i]->setOutputAudio(out);
  }

  /// Satisfies the AudioWriter/AudioDecoder interface - delegates to
  /// setOutputAudio(), matching every concrete Demuxer's own convention.
  void setOutput(Print &out) override { setOutputAudio(out); }

  /// Defines the video output - forwarded to every registered demuxer
  /// eagerly (see class comment).
  void setOutputVideo(Print &out) override {
    for (int i = 0; i < demuxers.size(); i++) demuxers[i]->setOutputVideo(out);
  }
  /// See Demuxer::setOutputVideo(VideoOutput&) - forwarded to every
  /// registered demuxer eagerly (same rationale as the Print& overload
  /// above).
  void setOutputVideo(VideoOutput &out) override {
    for (int i = 0; i < demuxers.size(); i++) demuxers[i]->setOutputVideo(out);
  }

  /// Forwarded to every registered demuxer eagerly (see class comment).
  void setSendWavHeader(bool flag) override {
    for (int i = 0; i < demuxers.size(); i++)
      demuxers[i]->setSendWavHeader(flag);
  }

  /// The container MIME type of the currently selected demuxer (e.g.
  /// "video/avi") - nullptr before the first write()/if none matched.
  const char *mimeVideo() override {
    return p_selected != nullptr ? p_selected->mimeVideo() : nullptr;
  }

  /// The audio MIME type of the currently selected demuxer's audio track
  /// - see Demuxer::mime(). nullptr before the first write()/if none
  /// matched, or (depending on the concrete demuxer) until enough of the
  /// audio track's own metadata has been parsed.
  const char *mime() override {
    return p_selected != nullptr ? p_selected->mime() : nullptr;
  }

  /// Common video info of the currently selected demuxer - an empty
  /// VideoInfo before the first write()/if none matched.
  VideoInfo getVideoInfo() override {
    return p_selected != nullptr ? p_selected->getVideoInfo() : VideoInfo{};
  }

  /// Common audio info of the currently selected demuxer - an empty
  /// AudioInfoFormat before the first write()/if none matched.
  AudioInfoFormat getAudioInfo() override {
    return p_selected != nullptr ? p_selected->getAudioInfo()
                                  : AudioInfoFormat{};
  }

  /// Defers actual demuxer init to the first write() (see class comment)
  /// - always succeeds itself.
  bool begin() override {
    p_selected = nullptr;
    is_first = true;
    return true;
  }

  void end() override {
    if (p_selected != nullptr) p_selected->end();
    p_selected = nullptr;
    is_first = true;
  }

  size_t write(const uint8_t *data, size_t len) override {
    if (len == 0) return 0;
    if (is_first) {
      is_first = false;
      for (int i = 0; i < demuxers.size(); i++) {
        if (demuxers[i]->isValid(data, len)) {
          select(demuxers[i]);
          break;
        }
      }
      if (p_selected == nullptr) {
        LOGE(
            "MultiVideoDemuxer: could not determine container format from "
            "first %u bytes",
            (unsigned)len);
      }
    }
    return p_selected != nullptr ? p_selected->write(data, len) : len;
  }

  operator bool() override {
    return p_selected != nullptr ? (bool)(*p_selected) : is_first;
  }

  /// The demuxer detection picked for the current stream - nullptr before
  /// the first write(), or if none matched.
  Demuxer *selectedDemuxer() const { return p_selected; }

 protected:
  Vector<Demuxer *> demuxers{0};
  Demuxer *p_selected = nullptr;
  bool is_first = true;

  void select(Demuxer *demuxer) {
    p_selected = demuxer;
    p_selected->begin();
    LOGI("MultiVideoDemuxer: selected demuxer for %s", demuxer->mimeVideo());
  }
};

}  // namespace audio_tools
