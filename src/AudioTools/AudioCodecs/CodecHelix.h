#pragma once

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/AudioCodecs/CodecAACHelix.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/AudioCodecs/CodecWAV.h"
#include "AudioTools/AudioCodecs/MultiDecoder.h"

namespace audio_tools {

/**
 * @brief MP3 and AAC Decoder using libhelix:
 * https://github.com/pschatzmann/arduino-libhelix. We dynamically create a MP3
 * or AAC decoder dependent on the provided audio format. In addition WAV files
 * are also supported
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class DecoderHelix : public MultiDecoder {
 public:
  DecoderHelix() {
    // register supported codecs with their mime type
    addDecoder(mp3, "audio/mpeg");
    addDecoder(aac, "audio/aac");
    addDecoder(wav, "audio/vnd.wave");
  }

 protected:
  MP3DecoderHelix mp3;
  AACDecoderHelix aac;
  WAVDecoder wav;
};

}  // namespace audio_tools
