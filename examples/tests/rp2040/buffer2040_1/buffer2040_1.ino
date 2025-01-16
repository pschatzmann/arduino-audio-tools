#include "AudioTools.h"
#include "AudioTools/Concurrency/RP2040.h"

BufferRP2040T<int> buffer(10, 20);  //20 blocks with 10 ints->100 ints

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  while (!Serial)
    ;
  delay(1000);
  Serial.println("testing...");

  assert(buffer.size() == 200);
  assert(buffer.available() == 0);
  assert(buffer.availableForWrite() == 200);
  for (int j = 0; j < 10; j++) {
    assert(buffer.write(j));
  }
  assert(buffer.size() == 200);
  assert(buffer.available() == 10);

  int tmp[20];
  assert(buffer.readArray(tmp, 20) == 10);

  for (int j = 0; j < 10; j++) {
    assert(tmp[j] == j);
  }

  assert(buffer.writeArray(tmp, 10) == 10);

  Serial.println("success!");
}

void loop() {
}