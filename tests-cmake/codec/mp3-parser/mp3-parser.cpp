/// Testing the MP3ParserEncoder

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/MP3Parser.h"
#include "AudioTools/Disk/AudioSourceSTD.h"
#include "AudioTools/AudioLibs/Desktop/File.h" 

MP3ParserEncoder enc;     // mp3 packaging
MetaDataFilterEncoder filter(enc);
CallbackStream cb;
EncodedAudioOutput out_stream(&cb, &filter);
AudioSourceSTD source("/home/pschatzmann/Music", ".mp3");
File file;
StreamCopy copier(out_stream, file);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  file = SD.open("/home/pschatzmann/Music/test.mp3", FILE_READ);
  out_stream.begin();
}


void loop() { copier.copy(); }
