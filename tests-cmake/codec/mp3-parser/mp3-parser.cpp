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

  auto write_cb = [](const uint8_t* data, size_t size) {
    char msg[120];
    snprintf(msg, 120, "write: %d, sample_rate: %d", (int)size, (int)enc.audioInfo().sample_rate);
    Serial.println(msg);
    return size;
  };
  cb.setWriteCallback(write_cb);
  cb.begin();
}


void loop() { if (!copier.copy()) exit(0); }
