#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMTS.h"

HexDumpOutput out;
MTSDecoder codecMTS;
FILE *fp;
uint8_t buffer[1024];

void setup() {
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);  

  fp = fopen(
      "/home/pschatzmann/Downloads/tmp/bbc_radio_one-audio=96000-272169555.ts",
      "rb");
  codecMTS.setOutput(out);
  codecMTS.begin();
  const size_t fileSize = fread(buffer, sizeof(unsigned char), 1024, fp);
  // Every 188th byte should be 0x47.
  assert(buffer[0] == 0x47);
  assert(buffer[188] == 0x47);
  codecMTS.write(buffer, fileSize);
}

void loop() {
  const size_t fileSize = fread(buffer, sizeof(unsigned char), 1024, fp);
  if (fileSize==0) {
    LOGI("End of File");
    stop();
  } 
  codecMTS.write(buffer, fileSize);
}