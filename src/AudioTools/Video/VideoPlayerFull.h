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
 * @brief VideoPlayer subclass pre-registered with every video/audio codec
 * this library ships a portable decoder for - H264/MJPEG/MPEG-1 video,
 * MP3/AAC/MP2 audio - so it's the "just point it at a file, any common
 * codec just works" player. Use instead of the base VideoPlayer unless
 * you want to keep these dependencies out of your build (the base
 * class's own multi-decoders start empty - see its class comment).
 *
 * Pre-registers no container demuxer though - still supply one via the
 * constructor/addDemuxer(), as the base VideoPlayer requires. Pass a
 * MultiVideoDemuxerFull (Video/MultiVideoDemuxerFull.h) for the same
 * "any format just works" convenience one layer down (AVI/MP4/MPG):
 * @code
 * MultiVideoDemuxerFull demuxer;
 * VideoPlayerFull player(demuxer, tftOutput, audioOut);
 * player.begin(file);
 * @endcode
 *
 * WAV is deliberately not included (no external dependency to justify
 * bundling it) - register it yourself if needed:
 * addAudioDecoder(wavDecoder, "audio/vnd.wave").
 *
 * Dependencies (install via Library Manager):
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

  /// `demuxer` is registered the same way VideoPlayer's own matching
  /// constructor does (see its class comment) - pass a
  /// MultiVideoDemuxerFull here instead of a single concrete demuxer if
  /// you want every container format supported too (see this class's own
  /// comment).
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
    // mp3_decoder must be registered before mp2_decoder: a plain
    // "audio/mpeg" match still resolves to it via MultiDecoder::
    // selectDecoder()'s base-mime-type fallback, which is correct since
    // MP2 only shows up in an MPEG-PS (.mpg) container, reporting the
    // more specific "...codecs=mpeg1-layer2" mime mp2_decoder is keyed on.
    addAudioDecoder(mp3_decoder, "audio/mpeg");
    addAudioDecoder(aac_decoder, "audio/aac");
    addAudioDecoder(mp2_decoder, "audio/mpeg; codecs=\"mpeg1-layer2\"");
  }
};

}  // namespace audio_tools
