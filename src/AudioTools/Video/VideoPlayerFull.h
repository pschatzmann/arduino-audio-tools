#pragma once

#include "AudioTools/AudioCodecs/CodecAACHelix.h"
#include "AudioTools/AudioCodecs/CodecMP2.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/Video/CodecH264.h"
#include "AudioTools/Video/CodecJPEG.h"
#include "AudioTools/Video/CodecMPG.h"
#include "AudioTools/Video/VideoPlayer.h"

namespace audio_tools {

/**
 * @brief VideoPlayer subclass configured for every codec this library
 * ships a portable software decoder for - video AND audio - so it's the
 * "just point it at a file, any common codec just works" player, the same
 * convenience MultiVideoDecoderFull/DecoderHelix each provide on their own
 * side. Use this instead of the base VideoPlayer unless you specifically
 * want to keep this class's extra dependencies (below) out of your build
 * - the base VideoPlayer's video and audio multi-decoders both start
 * empty (see its own class comment), so it has none.
 *
 * Video codecs - registered in this subclass's own constructor, the same
 * way MultiVideoDecoderFull's does (MultiVideoDecoderFull.h - a plain
 * MultiVideoDecoder pre-registered the same way, for callers that want
 * this convenience without the rest of VideoPlayer):
 * - H264 (H264Decoder, TinyH264)
 * - Motion-JPEG (MJPEGDecoder, TinyJPEG)
 * - MPEG-1 (MPGDecoder, TinyMPG)
 *
 * Audio codecs - also registered in this subclass's own constructor,
 * since the base VideoPlayer's audio multi-decoder starts empty on
 * purpose (see its class comment):
 * - "audio/mpeg" -> MP3DecoderHelix
 * - "audio/aac" -> AACDecoderHelix
 * - "audio/mpeg; codecs=\"mpeg1-layer2\"" -> MP2Decoder - the exact mime
 *   DemuxerMPG::mime() reports for Layer II (see its own comment); a
 *   plain "audio/mpeg" match (from any container) still resolves to
 *   MP3DecoderHelix instead (registered first - see MultiDecoder::
 *   selectDecoder()'s base-mime-type fallback), which is correct since
 *   MP2 realistically only shows up in an MPEG-PS (.mpg) container.
 *
 * WAV is deliberately not included - DemuxerAVI/DemuxerMP4 report it too
 * ("audio/vnd.wave"), but AudioTools/AudioCodecs/CodecWAV.h has no
 * external dependency of its own to justify bundling it here; register it
 * yourself (addAudioDecoder(wavDecoder, "audio/vnd.wave")) if your
 * content needs it.
 *
 * Dependencies (install via Library Manager) - all pulled in only by
 * choosing this subclass over the dependency-free base VideoPlayer:
 * - https://github.com/pschatzmann/TinyH264
 * - https://github.com/pschatzmann/TinyMPG
 * - https://github.com/pschatzmann/TinyJPEG
 * - https://github.com/pschatzmann/arduino-libhelix
 * - https://github.com/pschatzmann/TinyMP2
 *
 * @ingroup player
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VideoPlayerFull : public VideoPlayer {
 public:
  VideoPlayerFull() {
    registerVideoDecoders();
    registerAudioDecoders();
  }

  VideoPlayerFull(Demuxer& demuxer, VideoOutput& videoOutput)
      : VideoPlayer(demuxer, videoOutput) {
    registerVideoDecoders();
    registerAudioDecoders();
  }

  VideoPlayerFull(Demuxer& demuxer, VideoOutput& videoOutput,
                  AudioOutput& audioOutput)
      : VideoPlayer(demuxer, videoOutput, audioOutput) {
    registerVideoDecoders();
    registerAudioDecoders();
  }

  VideoPlayerFull(Demuxer& demuxer, VideoOutput& videoOutput,
                  Print& audioOutput)
      : VideoPlayer(demuxer, videoOutput, audioOutput) {
    registerVideoDecoders();
    registerAudioDecoders();
  }

  VideoPlayerFull(Demuxer& demuxer, VideoOutput& videoOutput,
                  AudioStream& audioOutput)
      : VideoPlayer(demuxer, videoOutput, audioOutput) {
    registerVideoDecoders();
    registerAudioDecoders();
  }

 protected:
  H264Decoder h264_decoder;
  MJPEGDecoder mjpeg_decoder;
  MPGDecoder mpeg_decoder;

  MP3DecoderHelix mp3_decoder;
  AACDecoderHelix aac_decoder;
  MP2Decoder mp2_decoder;

  void registerVideoDecoders() {
    addVideoDecoder(h264_decoder);
    addVideoDecoder(mjpeg_decoder);
    addVideoDecoder(mpeg_decoder);
  }

  void registerAudioDecoders() {
    addAudioDecoder(mp3_decoder, "audio/mpeg");
    addAudioDecoder(aac_decoder, "audio/aac");
    addAudioDecoder(mp2_decoder, "audio/mpeg; codecs=\"mpeg1-layer2\"");
  }
};

}  // namespace audio_tools
