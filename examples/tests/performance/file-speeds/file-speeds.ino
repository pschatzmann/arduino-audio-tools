#include "SPI.h"
#include "SD.h"
#include "SD_MMC.h"
#include <SdFat.h>
#include "AudioTools.h"
#define AUDIOBOARD_SD
#include "AudioTools/Disk/VFS_SDSPI.h"
#include "AudioTools/Disk/VFS_SDMMC.h"
#include "AudioTools/Disk/VFSFile.h"
#define PIN_AUDIO_KIT_SD_CARD_CS 13
#define PIN_AUDIO_KIT_SD_CARD_MISO 2
#define PIN_AUDIO_KIT_SD_CARD_MOSI 15
#define PIN_AUDIO_KIT_SD_CARD_CLK  14

Vector<uint8_t> data;
int len[] = { 1, 10, 100, 1024, 1024 * 10, 1024 * 100, 1024 * 1000 };
int totalSize = 1024 * 1024 * 100;

void testWrite(Stream& file, int writeSize, int totalSize, const char* name) {
  data.resize(writeSize);
  memset(data.data(), 0, data.size());
  int32_t start = millis();
  while (totalSize > 0) {
    int written = file.write(data.data(), min(writeSize, totalSize));
    totalSize -= written;
  }
  int32_t time = millis() - start;
  float thru = (float)totalSize / (float)time;
  Serial.printf("Write, %s, %d, %f\n", name, writeSize, thru);
}

void testRead(Stream& file, int writeSize, int totalSize, const char* name) {
  data.resize(writeSize);
  memset(data.data(), 0, data.size());
  int32_t start = millis();
  while (totalSize > 0) {
    int written = file.readBytes(data.data(), min(writeSize, totalSize));
    totalSize -= written;
  }
  int32_t time = millis() - start;
  float thru = (float)totalSize / (float)time;
  Serial.printf("Read, %s, %d, %f\n", name, writeSize, time);
}


template<typename SD, typename Open>
void testFS(SD& sd, Open write, Open read, int cs = -1) {
  bool ok = cs != -1 ? sd.begin(cs) : sd.begin();
  if (!ok){
    Serial.println("SD error");
    return;
  }

  auto file = sd.open("/test.txt", write);
  for (int i : len) {
    testWrite(file, len[i], totalSize, "VFS_SDMMC, Write");
  }
  file.close();
  file = sd.open("/sdmmc/test.txt", read);
  for (int i : len) {
    testRead(file, len[i], totalSize, "VFS_SDMMC, Read");
  }
  file.close();
  sd.end();
}

void setup() {
  VFS_SDSPI sd;
  VFS_SDMMC sdmmc;
  SdFat sdfat;

  Serial.begin(115200);

  SPI.begin(PIN_AUDIO_KIT_SD_CARD_CLK, PIN_AUDIO_KIT_SD_CARD_MISO, PIN_AUDIO_KIT_SD_CARD_MOSI, PIN_AUDIO_KIT_SD_CARD_CS);

  testFS<fs::SDFS, const char*>(SD, FILE_WRITE, FILE_READ, PIN_AUDIO_KIT_SD_CARD_CS);
  testFS<fs::SDMMCFS, const char*>(SD_MMC, FILE_WRITE, FILE_READ);
  testFS<SdFat, int>(sdfat, O_WRITE | O_CREAT, O_RDONLY, PIN_AUDIO_KIT_SD_CARD_CS);
  testFS<VFS_SDSPI, FileMode>(sd, VFS_FILE_WRITE, VFS_FILE_READ);
  testFS<VFS_SDMMC, FileMode>(sdmmc, VFS_FILE_WRITE, VFS_FILE_READ);
  stop();
}

void loop() {}