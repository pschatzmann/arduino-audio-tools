#define AUDIOBOARD_SD
#include "AudioTools.h"
#include "AudioTools/Disk/VFSFile.h"
#include "AudioTools/Disk/VFS_SDMMC.h"

const size_t max_len = 1024 * 100;
uint8_t *data = nullptr;
int len[] = { 1, 5, 10, 25, 100, 256, 512, 1024, 1024 * 10, 1024 * 100 };
size_t totalSize = 1024 * 1024 * 1;
const char* test_file = "/test.txt";

void testWrite(Stream& file, int writeSize, int totalSize) {
  memset(data, 0, max_len);
  int32_t start = millis();
  while (totalSize > 0) {
    int written = file.write(data, min(writeSize, totalSize));
    //Serial.println(written);
    //assert(written > 0);
    totalSize -= written;
  }
}

void testRead(Stream& file, int readSize, int totalSize) {
  memset(data, 0, max_len);
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
  while (!sd.begin()) {
    Serial.print(name);
    Serial.println(" error");
    delay(1000);
  }

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

  VFS_SDMMC sdmmc;

  data = new uint8_t[max_len];
  assert(data!=nullptr);

 // testFS<VFS_SDSPI, FileMode>("VFS_SDSPI", sd, VFS_FILE_WRITE, VFS_FILE_READ);
  testFS<VFS_SDMMC, FileMode>("VFS_SDMMC", sdmmc, VFS_FILE_WRITE, VFS_FILE_READ);
}

void loop() {}