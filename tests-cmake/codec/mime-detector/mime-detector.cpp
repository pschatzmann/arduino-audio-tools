/// Synthetic test: MimeDetector must reliably distinguish ADTS AAC from MP3
/// even though both formats share the same 0xFF../0xFFF.. sync pattern.

#include <assert.h>
#include <cstring>
#include <vector>

#include "AudioTools.h"
#include "AudioTools/CoreAudio/AudioMetaData/MimeDetector.h"

using namespace audio_tools;

// Builds a single 7 byte ADTS header (no CRC) followed by zeroed payload
// up to frame_length bytes.
static std::vector<uint8_t> buildADTSFrame(int frameLength, int profile = 1,
                                            int freqIdx = 4,
                                            int channelCfg = 2, int id = 0) {
  std::vector<uint8_t> f(frameLength, 0);
  f[0] = 0xFF;
  f[1] = 0xF0 | ((id & 1) << 3) | 0x01;  // layer always 0, protection_absent=1
  f[2] = ((profile & 0x3) << 6) | ((freqIdx & 0xF) << 2) |
         ((channelCfg >> 2) & 0x1);
  f[3] = ((channelCfg & 0x3) << 6) | ((frameLength >> 11) & 0x3);
  f[4] = (frameLength >> 3) & 0xFF;
  f[5] = ((frameLength & 0x7) << 5) | 0x1F;
  f[6] = 0x00;
  return f;
}

static std::vector<uint8_t> buildADTSStream(int frames, int frameLength = 80) {
  std::vector<uint8_t> out;
  for (int i = 0; i < frames; i++) {
    auto f = buildADTSFrame(frameLength);
    out.insert(out.end(), f.begin(), f.end());
  }
  return out;
}

// Builds a single MPEG-1 Layer III frame: version=11, layer=01, no CRC,
// bitrate index 9 (128kbps), sample rate index 0 (44100), stereo.
static std::vector<uint8_t> buildMP3Frame() {
  const int frameLength = 418;  // matches 128kbps/44100 CBR layer3
  std::vector<uint8_t> f(frameLength, 0);
  f[0] = 0xFF;
  f[1] = 0xE0 | (0x3 << 3) | (0x1 << 1) | 0x1;  // MPEG1, Layer3, no CRC
  f[2] = (9 << 4) | (0 << 2);                   // 128kbps, 44100Hz
  f[3] = 0x00;                                  // stereo
  return f;
}

static std::vector<uint8_t> buildMP3Stream(int frames) {
  std::vector<uint8_t> out;
  for (int i = 0; i < frames; i++) {
    auto f = buildMP3Frame();
    out.insert(out.end(), f.begin(), f.end());
  }
  return out;
}

// Builds a synthetic AC-3 frame: sync word 0x0B77 followed by a bsid field.
static std::vector<uint8_t> buildAC3Frame(uint8_t bsid, int frameLength = 128) {
  std::vector<uint8_t> f(frameLength, 0);
  f[0] = 0x0B;
  f[1] = 0x77;
  f[5] = (bsid & 0x1F) << 3;
  return f;
}

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // 1) A synthetic AAC/ADTS stream must be recognized as audio/aac and NOT
  // as audio/mpeg
  {
    auto aac = buildADTSStream(4, 80);
    assert(MimeDetector::checkAACExt(aac.data(), aac.size()) == true);
    assert(MimeDetector::checkMP3Ext(aac.data(), aac.size()) == false);
  }

  // 2) A synthetic MP3 stream must be recognized as audio/mpeg and NOT as
  // audio/aac
  {
    auto mp3 = buildMP3Stream(4);
    assert(MimeDetector::checkMP3Ext(mp3.data(), mp3.size()) == true);
    assert(MimeDetector::checkAACExt(mp3.data(), mp3.size()) == false);
  }

  // 3) End-to-end MimeDetector: aac stream -> audio/aac
  {
    MimeDetector detector;
    detector.begin();
    auto aac = buildADTSStream(4, 80);
    detector.write(aac.data(), aac.size());
    assert(strcmp(detector.mime(), "audio/aac") == 0);
  }

  // 4) End-to-end MimeDetector: mp3 stream -> audio/mpeg
  {
    MimeDetector detector;
    detector.begin();
    auto mp3 = buildMP3Stream(4);
    detector.write(mp3.data(), mp3.size());
    assert(strcmp(detector.mime(), "audio/mpeg") == 0);
  }

  // 5) A synthetic AC-3 frame must be recognized as audio/ac3 and NOT as
  // E-AC-3, AAC or MP3
  {
    auto ac3 = buildAC3Frame(8);  // bsid=8 -> standard AC-3
    assert(MimeDetector::checkAC3(ac3.data(), ac3.size()) == true);
    assert(MimeDetector::checkEAC3(ac3.data(), ac3.size()) == false);
    assert(MimeDetector::checkAACExt(ac3.data(), ac3.size()) == false);
    assert(MimeDetector::checkMP3Ext(ac3.data(), ac3.size()) == false);
  }

  // 6) A synthetic E-AC-3 frame must be recognized as audio/eac3 and NOT as
  // AC-3
  {
    auto eac3 = buildAC3Frame(16);  // bsid=16 -> E-AC-3
    assert(MimeDetector::checkEAC3(eac3.data(), eac3.size()) == true);
    assert(MimeDetector::checkAC3(eac3.data(), eac3.size()) == false);
  }

  // 7) End-to-end MimeDetector: AC-3 stream -> audio/ac3
  {
    MimeDetector detector;
    detector.begin();
    auto ac3 = buildAC3Frame(8);
    detector.write(ac3.data(), ac3.size());
    assert(strcmp(detector.mime(), "audio/ac3") == 0);
  }

  // 8) End-to-end MimeDetector: E-AC-3 stream -> audio/eac3
  {
    MimeDetector detector;
    detector.begin();
    auto eac3 = buildAC3Frame(16);
    detector.write(eac3.data(), eac3.size());
    assert(strcmp(detector.mime(), "audio/eac3") == 0);
  }

  Serial.println("All MimeDetector aac/mp3 discrimination tests passed");
  exit(0);
}

void loop() {}
