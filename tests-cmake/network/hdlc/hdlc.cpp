// Simple wrapper for Arduino sketch to compilable with cpp in cmake
#include "AudioTools.h"
#include "AudioTools/Communication/HDLCStream.h"

RingBuffer<uint8_t> ringBuffer(5 * 1024);
QueueStream<uint8_t> queueStream(ringBuffer);
HDLCStream hdlcStream(queueStream, 1024);

// Arduino Setup
void setup(void) {
  // Open Serial
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  queueStream.begin();
  Serial.println(ringBuffer.available());

  uint8_t data[1024];
  // write 100 bytes
  memset(data, 1, sizeof(data));
  size_t written = hdlcStream.write(data, 100);
  Serial.println(ringBuffer.available());
  assert(written == 100);

  // write 200 bytes
  memset(data, 2, sizeof(data));
  written = hdlcStream.write(data, 200);
  Serial.println(ringBuffer.available());
  assert(written == 200);

  // write 300 bytes
  memset(data, 3, sizeof(data));
  written = hdlcStream.write(data, 300);
  Serial.println(ringBuffer.available());
  assert(written == 300);

  // read back first write
  size_t read = hdlcStream.readBytes(data, 1000);
  Serial.println(read);
  for (int j=0;j<100;j++) assert(data[j] == 1);
  assert(read == 100);

  // read back second write
  read = hdlcStream.readBytes(data, 1000);
  Serial.println(read);
  for (int j=0;j<200;j++) assert(data[j] == 2);
  assert(read == 200);

  // read back third write
  read = hdlcStream.readBytes(data, 1000);
  Serial.println(read);
  for (int j=0;j<300;j++) assert(data[j] == 3);
  assert(read == 300);
  Serial.println("END");
}

// Arduino loop - copy sound to out
void loop() {}
