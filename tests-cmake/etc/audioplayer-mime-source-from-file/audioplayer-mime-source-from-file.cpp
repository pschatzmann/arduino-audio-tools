/// Test for AudioPlayerMimeSourceFromFile: a MimeSource that determines the mime
/// type from the extension of the file an AudioPlayer is currently playing
/// (rather than from content sniffing). Intended to be plugged into
/// MultiDecoder::setMimeSource() for disk based playback (SD, SD_MMC, ...).

#include <assert.h>
#include <cstring>

#include "AudioTools.h"
#include "AudioTools/Disk/AudioPlayerMimeSourceFromFile.h"

using namespace audio_tools;

/// Minimal Stream stand-in for a disk File, so this test does not depend on
/// SD/SD_MMC hardware or filesystem stacks.
class FakeFile : public Stream {
 public:
  FakeFile(const char* n = nullptr) : file_name(n) {}
  const char* name() { return file_name; }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  size_t write(uint8_t) override { return 0; }

 protected:
  const char* file_name;
};

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  AudioPlayer player;  // default ctor: no source/output/decoder needed here
  AudioPlayerMimeSourceFromFile<FakeFile> mimeSourceFromFile(player);

  // no stream selected yet -> no mime
  assert(mimeSourceFromFile.mime() == nullptr);

  // known extensions, case-insensitive
  {
    FakeFile f("09 - Track.MP3");
    player.setStream(&f);
    assert(strcmp(mimeSourceFromFile.mime(), "audio/mpeg") == 0);
  }
  {
    FakeFile f("song.flac");
    player.setStream(&f);
    assert(strcmp(mimeSourceFromFile.mime(), "audio/flac") == 0);
  }
  {
    FakeFile f("11.wav");
    player.setStream(&f);
    assert(strcmp(mimeSourceFromFile.mime(), "audio/vnd.wave") == 0);
  }
  {
    FakeFile f("clip.aac");
    player.setStream(&f);
    assert(strcmp(mimeSourceFromFile.mime(), "audio/aac") == 0);
  }

  // unknown extension / no extension -> no mime
  {
    FakeFile f("WPSettings.dat");
    player.setStream(&f);
    assert(mimeSourceFromFile.mime() == nullptr);
  }
  {
    FakeFile f("IndexerVolumeGuid");
    player.setStream(&f);
    assert(mimeSourceFromFile.mime() == nullptr);
  }

  // custom extension registered via the embedded MimeResolver
  {
    mimeSourceFromFile.mimeResolver().addMimeEntry("xyz", "audio/x-custom");
    FakeFile f("weird.xyz");
    player.setStream(&f);
    assert(strcmp(mimeSourceFromFile.mime(), "audio/x-custom") == 0);
  }

  Serial.println("All AudioPlayerMimeSourceFromFile tests passed");
  exit(0);
}

void loop() {}
