#pragma once
#include "AudioTools/AudioCodecs/CodecADTS.h"
#include "AudioTools/CoreAudio/AudioBasic/StrView.h"

namespace audio_tools {

/**
 * @brief  AAC header parser to check if the data is a valid ADTS aac which
 * can extract some relevant audio information.
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class HeaderParserAAC {
 public:
  /// parses the header string and returns true if this is a valid aac adts
  /// stream
  bool isValid(const uint8_t* data, int len) {
    if (len < 7) return false;
    parser.begin();
    // regular validation
    if (!parser.parse((uint8_t*)data)) return false;
    // check if we have a valid 2nd frame
    if (len > getFrameLength()) {
      int pos = findSyncWord(data, len, getFrameLength());
      if (pos == -1) return false;
    }
    return true;
  }

  int getSampleRate() { return parser.getSampleRate(); }

  uint8_t getChannels() { return parser.data().channel_cfg; }

  /// Determines the frame length
  int getFrameLength() { return parser.getFrameLength(); }

  /// Finds the mp3/aac sync word
  int findSyncWord(const uint8_t* buf, int nBytes, int start = 0) {
    return parser.findSyncWord(buf, nBytes, start);
  }

  ADTSParser::ADTSHeader getHeader() { return parser.data(); }

 protected:
  ADTSParser parser;
};

}  // namespace audio_tools