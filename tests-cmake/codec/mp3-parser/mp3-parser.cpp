/// Testing the MP3ParserEncoder: the mp3 data must start with a sync word

#include <assert.h>

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/MP3Parser.h"
#include "AudioTools/AudioLibs/Desktop/File.h"

MP3ParserEncoder enc;  // mp3 packaging
MetaDataFilterEncoder filter(enc);
CallbackStream cb;
EncodedAudioOutput out_stream(&cb, &filter);
File file;
StreamCopy copier(out_stream, file);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  file = SD.open("/home/pschatzmann/Music/test.mp3", FILE_READ);
  out_stream.begin();

  auto write_cb = [](const uint8_t* data, size_t size) {
    // Ensure each emitted chunk starts with an MP3 sync word (0xFFE?)
    if (size >= 2) {
      assert(data[0] == 0xFF && (data[1] & 0xE0) == 0xE0);
    }
    char msg[120];
    snprintf(msg, 120,
             "write: %d, sample_rate: %d, samples: %d, duration: %u us",
             (int)size, (int)enc.audioInfo().sample_rate, enc.samplesPerFrame(),
             (unsigned)enc.frameDurationUs());
    Serial.println(msg);
    return size;
  };
  cb.setWriteCallback(write_cb);
  cb.begin();
}

void loop() {
  if (!copier.copy()) {
    out_stream.end();
    exit(0);
  }
}
