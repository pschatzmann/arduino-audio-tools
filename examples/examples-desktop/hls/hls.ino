#include "AudioTools.h"
#include "AudioCodecs/CodecMTS.h"
#include "AudioCodecs/CodecAACHelix.h"


// HLSStream hls(out,"SSID", "PWD");
const int buffer_size = 1024;
CsvOutput<int16_t> out(Serial, 2);  // Or use StdOuput
MTSDecoder decoder;
AACDecoderHelix aac;
EncodedAudioStream dec_str(&out, &aac); // Decoding stream
EncodedAudioStream mts_str(&dec_str, &decoder);

FILE *file;
uint8_t buffer[buffer_size];

// Arduino Setup
void setup(void) {
  // Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Debug);

  file = fopen("/home/pschatzmann/Downloads/test.ts", "rb");
  dec_str.begin();
  mts_str.begin();
}

// Arduino loop
void loop() {
  size_t fileSize = fread(buffer, 1, buffer_size, file);
  if (fileSize > 0) {
    int written = mts_str.write(buffer, fileSize);
    LOGI("written %d, ", written);
    fileSize = fread(buffer, 1, buffer_size, file);
  }
}