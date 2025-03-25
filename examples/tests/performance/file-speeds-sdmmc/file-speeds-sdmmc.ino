#include "SPI.h"
#include "SD.h"
#include "SD_MMC.h"

#define PIN_AUDIO_KIT_SD_CARD_CS 13
#define PIN_AUDIO_KIT_SD_CARD_MISO 2
#define PIN_AUDIO_KIT_SD_CARD_MOSI 15
#define PIN_AUDIO_KIT_SD_CARD_CLK 14

uint8_t data[1024 * 100];
int len[] = { 1, 5, 10, 25, 100, 256, 512, 1024, 1024 * 10, 1024 * 100 };
size_t totalSize = 1024 * 1024 * 1;
const char* test_file = "/test.txt";

void testWrite(Stream& file, int writeSize, int totalSize) {
  memset(data, 0, sizeof(data));
  int32_t start = millis();
  while (totalSize > 0) {
    int written = file.write(data, min(writeSize, totalSize));
    //Serial.println(written);
    //assert(written > 0);
    totalSize -= written;
  }
}

void testRead(Stream& file, int readSize, int totalSize) {
  memset(data, 0, sizeof(data));
  while (totalSize > 0) {
    int read = file.readBytes(data, min(readSize, totalSize));
    //assert(read>0);
    totalSize -= read;
  }
}

void logTime(uint32_t start, int i, const char* name, const char* op) {
  int32_t time = millis() - start;
  float thru = (float)totalSize / (float)time / 1000.0;
  Serial.printf("%s, %s, %d, %d, %f\n", op, name, i, time, thru);
}

template<typename SD, typename Open>
void testFS(const char* name, SD& sd, Open write, Open read) {

  for (int i : len) {
    int32_t start = millis();
    auto file = sd.open(test_file, write);
    file.seek(0);
    assert(file);
    testWrite(file, i, totalSize);
    file.close();
    logTime(start, i, name, "Write");
  }
  for (int i : len) {
    int32_t start = millis();
    auto file = sd.open(test_file, read);
    assert(file);
    testRead(file, i, totalSize);
    file.close();
    logTime(start, i, name, "Read");
  }
  sd.end();
}


void setup() {
  Serial.begin(115200);

  while (!SD_MMC.begin()) {
    Serial.println("SDMMC error");
    delay(1000);
  }
  testFS<fs::SDMMCFS, const char*>("SD_MMC", SD_MMC, FILE_WRITE, FILE_READ);
}

void loop() {}