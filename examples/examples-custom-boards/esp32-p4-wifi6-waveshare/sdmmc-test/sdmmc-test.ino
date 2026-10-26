/**
 * @file sdmmc-test.ino
 * @brief MicroSD (TF) card test for the Waveshare ESP32-P4-WIFI6, over
 * SD_MMC in 1-bit mode.
 *
 * No pin or power setup is needed: arduino-esp32's ESP32-P4 defaults
 * (SDMMC slot 0 IOMUX pins CLK=43, CMD=44, D0-D3=39-42, power from on-chip
 * LDO channel 4) match this board's wiring.
 *
 * 4-bit mode mounts and reads fine on this board, but writes fail with CRC
 * errors (0x109) even at 10MHz, so this uses 1-bit mode, which works.
 *
 * Writes a small test file, reads it back, and lists the card's root
 * directory.
 *
 * Dependencies:
 * - none beyond the Arduino-ESP32 core (`SD_MMC`)
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include <FS.h>
#include <SD_MMC.h>

const char* testFile = "/audiotools_sdmmc_test.txt";
const char* testContent = "arduino-audio-tools sdmmc-test\n";

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

  if (!SD_MMC.begin("/sdcard", true)) {  // true = 1-bit mode, see header
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
