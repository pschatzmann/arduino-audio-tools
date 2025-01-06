#include "AudioTools.h"
#include "AudioTools/Concurrency/RP2040.h"

SpinLock mutex;
NBuffer<int16_t> nbuffer(512, 10);
SynchronizedBuffer<int16_t> buffer(nbuffer, mutex);

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("starting...");
}

void loop() {
  int16_t data[512];
  for (int j = 0; j < 512; j++) {
    data[j] = j;
  }
  size_t result = buffer.writeArray(data, 512);
  if(result != 512){
    // queue is full: give the reading queue some chance to empty
    delay(5);
  } 
}

void loop1() {
  static uint64_t start = millis();
  static size_t total_bytes = 0;
  static size_t errors = 0;
  static int16_t data[512];

  // read data
  size_t result = buffer.readArray(data, 512);
  if(result == 0){
    // reading queue is empty: give some time to fill
    delay(5);
    return;
  }

  // check data
  for (int j = 0; j < 512; j++) {
    if (data[j] != j) errors++;
  }
  // calculate bytes per second
  total_bytes += sizeof(int16_t) * 512;
  if (total_bytes >= 1024000) {
    uint64_t duration = millis() - start;
    float mbps = static_cast<float>(total_bytes) / duration / 1000.0;

    // print result
    Serial.print("Mbytes per second: ");
    Serial.print(mbps);
    Serial.print(" with ");
    Serial.print(errors);
    Serial.println(" errors");

    start = millis();
    errors = 0;
    total_bytes = 0;
  }
}