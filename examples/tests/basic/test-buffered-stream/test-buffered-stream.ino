/**
 * Testing peek and readBytes of BufferedStream
 * - fill DynamicMemoryStream with data
 * - check data of DynamicMemoryStream
 * - peek data
 * - check data vis BufferedStream
 */
#include "AudioTools.h"

DynamicMemoryStream data;
BufferedStream buffered(data);
HexDumpOutput out;
const int processingSize = 1024;
uint8_t buffer[processingSize];

void fillData() {
  Serial.println("Filling data...");
  for (int j = 0; j < processingSize; j++) {
    buffer[j] = j % 256;
  }
  while (data.size() < processingSize * 10) {
    assert(data.write(buffer, processingSize) == processingSize);
  }
}

void testPeek() {
  int len = buffered.peekBytes(&buffer[0], 160);
  Serial.println("Peek:");
  out.write(buffer, len);
  Serial.println();
}

void checkData(Stream &data, bool isPrint = false) {
  // check data
  Serial.print("TestingData");
  int n = data.readBytes(buffer, processingSize);
  Serial.print(" ");
  Serial.println(n);
  while (n == processingSize) {
    for (int j = 0; j < processingSize; j++) {
      uint8_t expected = j % 256;
      if (isPrint) {
        Serial.print(buffer[j]);
        Serial.print(", ");
        Serial.println(expected);
      }
      assert(buffer[j] == expected);
      n = data.readBytes(buffer, processingSize);
      assert(n == 0 || n == processingSize);
    }
  }
  Serial.println("test OK!");
}

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  delay(5000);

  data.begin();
  fillData();
  testPeek();
  checkData(data);
  
  data.rewind();
  checkData(buffered);
}

void loop() {
}