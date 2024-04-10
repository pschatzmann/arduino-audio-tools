#include "AudioTools.h"

#define SAMPLE_COUNT 2048
#define SAMPLE_BUFFER_SIZE sizeof(int32_t) * SAMPLE_COUNT
unsigned char raw_data[SAMPLE_BUFFER_SIZE];
AudioInfo info(8000, 1, 32);
I2SStream i2sStream;  // Access I2S as stream
MemoryStream raw_samples(raw_data, SAMPLE_BUFFER_SIZE, true, RAM);
StreamCopy copier(raw_samples, i2sStream, 1024);  // copies sound

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  auto cfg = i2sStream.defaultConfig(RX_MODE);
  cfg.copyFrom(info);
  i2sStream.begin(cfg);

  raw_samples.begin();

  copier.copy();

  assert(raw_samples.available()== 1024);
  assert(raw_samples.availableForWrite()== 7168);

}

void loop() {
  // put your main code here, to run repeatedly:
}
