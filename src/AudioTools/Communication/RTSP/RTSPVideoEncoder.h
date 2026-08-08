/*
 * Author: Phil Schatzmann
 */

#pragma once

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "IMediaSource.h"
#include "RTSPFormat.h"

namespace audio_tools {

/**
 * @brief Common base for RTP video payloaders (JPEGRtpEncoder,
 * H264RtpEncoder, ...): an AudioEncoder that accepts complete video frames
 * via write() and, as a packetized IMediaSource, hands the resulting RTP
 * fragments back out through packetSize()/readBytes().
 *
 * Letting RTSPOutput hold a single RTSPVideoEncoder* - and take a single
 * RTSPVideoEncoder& constructor parameter - instead of one pointer/overload
 * per codec keeps it from growing a new member/constructor pair for every
 * additional video format. Any codec-specific data a format needs from its
 * encoder (e.g. H.264's sprop-parameter-sets) goes through virtual methods
 * on RTSPFormat itself (see setSpropParameterSets()/setProfileLevelId()),
 * so encoders only ever need to hold a generic RTSPFormat*.
 *
 * @ingroup rtsp
 * @ingroup codecs
 * @author Phil Schatzmann
 */
class RTSPVideoEncoder : public AudioEncoder, public IMediaSource {
 public:
  /// Associates the RTSPFormat that supplies width/height/framerate/etc.
  /// and, for some codecs, receives codec-specific SDP data back.
  virtual void setFormat(RTSPFormat &format) = 0;
};

}  // namespace audio_tools
