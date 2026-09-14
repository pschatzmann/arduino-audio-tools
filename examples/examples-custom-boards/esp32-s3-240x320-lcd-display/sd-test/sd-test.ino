/**
 * @file sd-test.ino
 * @brief MicroSD card test for the Hosyond 2.8" ESP32-S3 Display. The slot
 * is wired as 4-bit SDIO (not SPI) on non-default pins, so it needs
 * SD_MMC.setPins() before begin() - see ESP32S3HosyondDisplay.h in
 * arduino-audio-driver for these pin numbers (shared with that board's
 * addSDMMC() bring-up).
 *
 * Writes a small test file, reads it back, and lists the card's root
 * directory.
 *
 * @see https://www.lcdwiki.com/2.8inch_ESP32-S3_Display
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include <FS.h>
#include <SD_MMC.h>

constexpr int SD_MMC_SCK = 38;
constexpr int SD_MMC_CMD = 40;
constexpr int SD_MMC_D0 = 39;
constexpr int SD_MMC_D1 = 41;
constexpr int SD_MMC_D2 = 48;
constexpr int SD_MMC_D3 = 47;

const char* testFile = "/audiotools_sd_test.txt";
const char* testContent = "arduino-audio-tools sd-test\n";

void listDir(const char* path) {
  File root = SD_MMC.open(path);
  File entry;
  while ((entry = root.openNextFile())) {
    Serial.printf("  %s%s  (%u bytes)\n", path, entry.name(), entry.size());
    entry.close();
  }
}

bool writeReadTest() {
  File f = SD_MMC.open(testFile, FILE_WRITE);
  if (!f) {
    Serial.println("open for write failed");
    return false;
  }
  f.print(testContent);
  f.close();

  f = SD_MMC.open(testFile, FILE_READ);
  if (!f) {
    Serial.println("open for read failed");
    return false;
  }
  String readBack = f.readString();
  f.close();

  Serial.printf("read back: %s", readBack.c_str());
  return readBack == testContent;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  Serial.println("starting SD_MMC...");

  if (!SD_MMC.setPins(SD_MMC_SCK, SD_MMC_CMD, SD_MMC_D0, SD_MMC_D1,
                       SD_MMC_D2, SD_MMC_D3)) {
    Serial.println("SD_MMC.setPins() failed");
    return;
  }
  if (!SD_MMC.begin()) {
    Serial.println("SD_MMC.begin() failed - check card is inserted");
    return;
  }

  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("no SD card found");
    return;
  }
  Serial.printf("card type: %d, size: %llu MB\n", cardType,
                SD_MMC.cardSize() / (1024 * 1024));

  Serial.println(writeReadTest() ? "write/read test: OK"
                                  : "write/read test: FAILED");

  Serial.println("root directory:");
  listDir("/");
}

void loop() {}
