#include "AudioTools.h"

// test different buffer implementations

void test(BaseBuffer<int16_t>& b, const char* title) {
  Serial.println(title);
  assert(b.isEmpty());
  for (int j = 0; j < 200; j++) {
    assert(b.write(j));
  }
  assert(b.isFull());
  for (int j = 0; j < 200; j++) {
    int16_t v = b.read();
    assert(v == j);
  }
  assert(b.isEmpty());
  int16_t array[200];
  for (int j = 0; j < 200; j++) {
    array[j] = j;
  }
  b.clear();
  int len = b.writeArray(array, 200);
  Serial.println(len);
  len = b.readArray(array,200);
  Serial.println(len);
  for (int j=0;j<200;j++){
      assert(array[j]==j);
  }

  Serial.println("Test OK");
}

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  SingleBuffer<int16_t> b1(200);
  test(b1, "SingleBuffer");

  RingBuffer<int16_t> b2(200);
  test(b2,"RingBuffer");

  NBuffer<int16_t> b3(50, 4);
  test(b3,"NBuffer");

  Serial.println("Tests OK");
}

void loop() {}