/**
 * @file sdmmc-test.ino
 * @brief MicroSD card test for the Guition ESP32-S3 4.3" 480x272 display,
 * accessed over SD_MMC (1-bit mode) - confirmed working on real hardware
 * (2026-08-19). See sd-test.ino in the sibling folder for the same card,
 * same physical pins, accessed over plain SPI instead - SD cards support
 * both interfaces on the same 4 pins (CLK, CMD/MOSI, DAT0/MISO, DAT3/CS),
 * which is also why the pins below were originally given in SPI
 * terminology even though this file uses SD_MMC. DAT3 (labelled "CS" by
 * the user, GPIO10) isn't one of the pins ESP32-S3's SD_MMC.setPins(clk,
 * cmd, d0) 1-bit overload manages, and driving it manually (either level)
 * was confirmed to make no difference on real hardware - so it's left
 * unconnected/unmanaged here.
 *
 * Writes a small test file, reads it back, and lists the card's root
 * directory.
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include <FS.h>
#include <SD_MMC.h>

// Confirmed working on real hardware, 2026-08-19.
constexpr int kPinSdClk = 12;
constexpr int kPinSdCmd = 11;
constexpr int kPinSdD0 = 13;

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

  if (!SD_MMC.setPins(kPinSdClk, kPinSdCmd, kPinSdD0)) {
    Serial.println("SD_MMC.setPins() failed");
    return;
  }
  if (!SD_MMC.begin("/sdcard", true)) {
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
