/// Synthetic test: MultiVideoDemuxer must auto-select the right registered
/// Demuxer (DemuxerAVI/DemuxerMP4/DemuxerMPG) from each container's own
/// signature bytes, and report "detection failed" cleanly when none match.
/// No real media file needed - only each format's first few header bytes
/// (checked by the concrete Demuxer::isValid() overrides) actually matter
/// for the selection logic under test here.

#include <assert.h>
#include <cstring>
#include <vector>

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/ContainerAVI.h"
#include "AudioTools/AudioCodecs/ContainerMP4.h"
#include "AudioTools/AudioCodecs/ContainerMPG.h"
#include "AudioTools/Video/MultiVideoDemuxer.h"
#include "AudioTools/Video/MultiVideoDemuxerFull.h"

using namespace audio_tools;

// Minimal RIFF/AVI signature ("RIFF" + size + "AVI "), zero-padded so a
// deeper parse attempt has buffered bytes to work with instead of running
// past what's available.
static std::vector<uint8_t> buildAVI() {
  std::vector<uint8_t> b(64, 0);
  memcpy(&b[0], "RIFF", 4);
  memcpy(&b[8], "AVI ", 4);
  return b;
}

// Minimal ISO base media 'ftyp' box (size + "ftyp" + major brand), zero-
// padded likewise.
static std::vector<uint8_t> buildMP4() {
  std::vector<uint8_t> b(64, 0);
  memcpy(&b[4], "ftyp", 4);
  memcpy(&b[8], "isom", 4);
  return b;
}

// Minimal MPEG Program Stream pack_start_code (00 00 01 BA), zero-padded.
static std::vector<uint8_t> buildMPG() {
  std::vector<uint8_t> b(64, 0);
  b[0] = 0x00;
  b[1] = 0x00;
  b[2] = 0x01;
  b[3] = 0xBA;
  return b;
}

// Matches none of the three container signatures.
static std::vector<uint8_t> buildGarbage() {
  return std::vector<uint8_t>(64, 0x42);
}

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Error);

  auto avi = buildAVI();
  auto mp4 = buildMP4();
  auto mpg = buildMPG();
  auto garbage = buildGarbage();

  // 1) Each concrete Demuxer's own isValid() must recognize its own
  // signature and reject the other two.
  {
    DemuxerAVI aviDemuxer;
    DemuxerMP4 mp4Demuxer;
    DemuxerMPG mpgDemuxer;

    assert(aviDemuxer.isValid(avi.data(), avi.size()) == true);
    assert(aviDemuxer.isValid(mp4.data(), mp4.size()) == false);
    assert(aviDemuxer.isValid(mpg.data(), mpg.size()) == false);

    assert(mp4Demuxer.isValid(mp4.data(), mp4.size()) == true);
    assert(mp4Demuxer.isValid(avi.data(), avi.size()) == false);
    assert(mp4Demuxer.isValid(mpg.data(), mpg.size()) == false);

    assert(mpgDemuxer.isValid(mpg.data(), mpg.size()) == true);
    assert(mpgDemuxer.isValid(avi.data(), avi.size()) == false);
    assert(mpgDemuxer.isValid(mp4.data(), mp4.size()) == false);
  }

  // 2) MultiVideoDemuxer selects DemuxerAVI for AVI content.
  {
    DemuxerAVI aviDemuxer;
    DemuxerMP4 mp4Demuxer;
    DemuxerMPG mpgDemuxer;
    MultiVideoDemuxer multi;
    multi.addDemuxer(aviDemuxer);
    multi.addDemuxer(mp4Demuxer);
    multi.addDemuxer(mpgDemuxer);

    multi.begin();
    assert((bool)multi == true);  // nothing tried yet
    assert(multi.selectedDemuxer() == nullptr);

    multi.write(avi.data(), avi.size());
    assert(multi.selectedDemuxer() == &aviDemuxer);
    assert(strcmp(multi.mimeVideo(), "video/avi") == 0);
  }

  // 3) MultiVideoDemuxer selects DemuxerMP4 for MP4 content.
  {
    DemuxerAVI aviDemuxer;
    DemuxerMP4 mp4Demuxer;
    DemuxerMPG mpgDemuxer;
    MultiVideoDemuxer multi;
    multi.addDemuxer(aviDemuxer);
    multi.addDemuxer(mp4Demuxer);
    multi.addDemuxer(mpgDemuxer);

    multi.begin();
    multi.write(mp4.data(), mp4.size());
    assert(multi.selectedDemuxer() == &mp4Demuxer);
    assert(strcmp(multi.mimeVideo(), "video/mp4") == 0);
  }

  // 4) MultiVideoDemuxer selects DemuxerMPG for MPEG-PS content.
  {
    DemuxerAVI aviDemuxer;
    DemuxerMP4 mp4Demuxer;
    DemuxerMPG mpgDemuxer;
    MultiVideoDemuxer multi;
    multi.addDemuxer(aviDemuxer);
    multi.addDemuxer(mp4Demuxer);
    multi.addDemuxer(mpgDemuxer);

    multi.begin();
    multi.write(mpg.data(), mpg.size());
    assert(multi.selectedDemuxer() == &mpgDemuxer);
    assert(strcmp(multi.mimeVideo(), "video/mpeg") == 0);
  }

  // 5) Content matching none of the registered demuxers is reported as a
  // clean detection failure, not a crash/wrong pick.
  {
    DemuxerAVI aviDemuxer;
    DemuxerMP4 mp4Demuxer;
    DemuxerMPG mpgDemuxer;
    MultiVideoDemuxer multi;
    multi.addDemuxer(aviDemuxer);
    multi.addDemuxer(mp4Demuxer);
    multi.addDemuxer(mpgDemuxer);

    multi.begin();
    multi.write(garbage.data(), garbage.size());
    assert(multi.selectedDemuxer() == nullptr);
    assert(multi.mimeVideo() == nullptr);
    assert((bool)multi == false);
  }

  // 6) addDemuxer() replaces, rather than adds to, an entry already
  // registered under the same mimeVideo() - the second DemuxerAVI must
  // win, matching MultiVideoDecoder::addDecoder()'s own dedup rule.
  {
    DemuxerAVI aviDemuxer1;
    DemuxerAVI aviDemuxer2;
    MultiVideoDemuxer multi;
    multi.addDemuxer(aviDemuxer1);
    multi.addDemuxer(aviDemuxer2);

    multi.begin();
    multi.write(avi.data(), avi.size());
    assert(multi.selectedDemuxer() == &aviDemuxer2);
  }

  // 7) MultiVideoDemuxerFull pre-registers all three formats - no manual
  // addDemuxer() calls needed to detect any of them.
  {
    MultiVideoDemuxerFull full;
    full.begin();
    full.write(mpg.data(), mpg.size());
    assert(full.selectedDemuxer() != nullptr);
    assert(strcmp(full.mimeVideo(), "video/mpeg") == 0);
  }

  Serial.println("All MultiVideoDemuxer detection tests passed");
  exit(0);
}

void loop() {}
