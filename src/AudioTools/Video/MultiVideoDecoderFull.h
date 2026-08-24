#pragma once

#include "AudioTools/Video/CodecH264.h"
#include "AudioTools/Video/CodecJPEG.h"
#include "AudioTools/Video/CodecMPG.h"
#include "AudioTools/Video/MultiVideoDecoder.h"

namespace audio_tools {

/**
 * @brief MultiVideoDecoder pre-registered with every video codec this
 * library ships a portable (no hardware-specific backend) software
 * decoder for: MJPEGDecoder (Motion-JPEG, TinyJPEG, CodecJPEG.h),
 * MPGDecoder (MPEG-1, TinyMPG, CodecMPG.h), H264Decoder (H.264 Annex-B,
 * TinyH264, CodecH264.h) - drop it into a demuxer's setOutputVideo() the
 * same way any single decoder would go, and it self-selects the right one
 * from the bitstream's own framing instead of the caller having to know
 * the codec up front. Use the plain MultiVideoDecoder (MultiVideoDecoder.h)
 * instead if you don't want all three codec libraries pulled in
 * unconditionally - register only what your content actually needs via
 * its own addDecoder().
 *
 * Dependencies (install via Library Manager) - all three are required to
 * build this header, regardless of which formats your content actually
 * uses:
 * - https://github.com/pschatzmann/TinyH264
 * - https://github.com/pschatzmann/TinyMPG
 * - https://github.com/pschatzmann/TinyJPEG
 *
 * @ingroup video
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MultiVideoDecoderFull : public MultiVideoDecoder {
 public:
  MultiVideoDecoderFull() {
    addDecoder(mjpeg);
    addDecoder(mpeg1);
    addDecoder(h264);
  }

 protected:
  MJPEGDecoder mjpeg;
  MPGDecoder mpeg1;
  H264Decoder h264;
};

}  // namespace audio_tools
