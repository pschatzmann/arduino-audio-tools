#pragma once

#include "AudioTools/AudioCodecs/ContainerAVI.h"
#include "AudioTools/AudioCodecs/ContainerMP4.h"
#include "AudioTools/AudioCodecs/ContainerMPG.h"
#include "AudioTools/Video/MultiVideoDemuxer.h"

namespace audio_tools {

/**
 * @brief MultiVideoDemuxer pre-registered with every container format
 * this library ships a demuxer for: DemuxerAVI (RIFF/AVI,
 * AudioCodecs/ContainerAVI.h), DemuxerMP4 (ISO base media/MP4,
 * AudioCodecs/ContainerMP4.h), DemuxerMPG (MPEG Program Stream,
 * AudioCodecs/ContainerMPG.h) - drop it in wherever a plain Demuxer& is
 * expected (VideoPlayer in fact uses one internally as its own built-in
 * demuxer) and it self-selects the right one from the stream's own
 * container signature instead of the caller having to know the file
 * format up front. Use the plain MultiVideoDemuxer
 * (MultiVideoDemuxer.h) instead if you don't want all three container
 * parsers pulled in unconditionally - register only what your content
 * actually needs via its own addDemuxer().
 *
 * Unlike MultiVideoDecoderFull's codecs, none of DemuxerAVI/DemuxerMP4/
 * DemuxerMPG wrap an external codec library - all three are pure parsing
 * code within this library itself, so this header adds no additional
 * third-party dependency beyond what MultiVideoDemuxer.h already needs.
 *
 * @ingroup codecs
 * @ingroup decoder
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MultiVideoDemuxerFull : public MultiVideoDemuxer {
 public:
  MultiVideoDemuxerFull() {
    addDemuxer(avi);
    addDemuxer(mp4);
    addDemuxer(mpg);
  }

 protected:
  DemuxerAVI avi;
  DemuxerMP4 mp4;
  DemuxerMPG mpg;
};

}  // namespace audio_tools
