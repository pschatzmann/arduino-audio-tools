#include "SPI.h"
#include "SdFat.h"

#define PIN_AUDIO_KIT_SD_CARD_CS 13
#define PIN_AUDIO_KIT_SD_CARD_MISO 2
#define PIN_AUDIO_KIT_SD_CARD_MOSI 15
#define PIN_AUDIO_KIT_SD_CARD_CLK 14
#define SPI_CLOCK SD_SCK_MHZ(10)

uint8_t* data = nullptr;
int len[] = { 1, 5, 10, 25, 100, 256, 512, 1024, 1024 * 10, 1024 * 100 };
size_t totalSize = 1024 * 1024 * 1;
const char* test_file = "/test.txt";

void testWrite(Stream& file, int writeSize, int totalSize) {
  memset(data, 0xff, sizeof(data));
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
  SdSpiConfig cfg(PIN_AUDIO_KIT_SD_CARD_CS, DEDICATED_SPI, SPI_CLOCK, &SPI);
  while (!sd.begin(cfg)) {
    Serial.print(name);
    Serial.println(" error");
    delay(1000);
  }

  for (int i : len) {
    int32_t start = millis();
    auto file = sd.open(test_file, write);
    assert(file);
    file.seek(0);
    testWrite(file, len[i], totalSize);
    file.close();
    logTime(start, i, name, "Write");
  }
  for (int i : len) {
    int32_t start = millis();
    auto file = sd.open(test_file, read);
    assert(file);
    testRead(file, len[i], totalSize);
    file.close();
    logTime(start, i, name, "Read");
  }
  sd.end();
}

void setup() {
  Serial.begin(115200);
  // allocate read/write buffer
  data = new uint8_t[1024 * 100];
  assert(data != nullptr);

  // setup SPI pins
  SPI.begin(PIN_AUDIO_KIT_SD_CARD_CLK, PIN_AUDIO_KIT_SD_CARD_MISO, PIN_AUDIO_KIT_SD_CARD_MOSI, PIN_AUDIO_KIT_SD_CARD_CS);
  delay(1000);
  
  // test SD
  SdFat sdfat;
  testFS<SdFat, int>("SDFAT", sdfat, FILE_WRITE, FILE_READ);
}

void loop() {}