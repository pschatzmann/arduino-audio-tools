#include "AudioTools.h"
#include "AudioTools/Disk/AudioSourceVFS.h"
#include "AudioTools/Disk/VFS_SDSPI.h"

//  We use an AudioKit or LyraT for the tests which uses the following pins
//  CS,  MOSI,  MISO,  SCK
VFS_SDSPI sd(13, 15, 2, 14);
AudioSourceVFS source(sd, "/sdcard", ".mp3");

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  source.begin();
  for (int j = 0; j < 10; j++) {
    VFSFile* p_file = (VFSFile*)source.selectStream(j);
    if (p_file)
      Serial.println(p_file->name());
  }
}

void loop() {}