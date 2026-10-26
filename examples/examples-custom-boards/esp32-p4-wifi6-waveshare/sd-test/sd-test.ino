/**
 * @file sd-test.ino
 * @brief MicroSD (TF) card test for the Waveshare ESP32-P4-WIFI6, accessed
 * over plain SPI (SD.h) rather than SD_MMC. See sdmmc-test.ino for the same
 * card over SD_MMC - it's the same physical pins either way (SD cards support
 * both interfaces on the same wiring: CLK, CMD/MOSI, DAT0/MISO, DAT3/CS).
 *
 * The SD pins (GPIO 39-48) are in the IO bank powered by on-chip LDO
 * channel 4; arduino-esp32 enables it automatically when these pins are used
 * (BOARD_PERIMAN_IO_LDO_AUTO), so no power setup is needed here.
 *
 * Writes a small test file, reads it back, and lists the card's root
 * directory.
 *
 * Dependencies:
 * - none beyond the Arduino-ESP32 core (`SD`, `SPI`)
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include <FS.h>
#include <SD.h>
#include <SPI.h>

// Same physical pins as sdmmc-test.ino, in SPI mode.
constexpr int kPinSdCs = 42;    // DAT3 in SD_MMC mode
constexpr int kPinSdMosi = 44;  // CMD in SD_MMC mode
constexpr int kPinSdClk = 43;
constexpr int kPinSdMiso = 39;  // DAT0 in SD_MMC mode

const char* testFile = "/audiotools_sd_test.txt";
const char* testContent = "arduino-audio-tools sd-test\n";

void listDir(const char* path) {
  File root = SD.open(path);
  File entry;
  while ((entry = root.openNextFile())) {
    Serial.printf("  %s%s  (%u bytes)\n", path, entry.name(), entry.size());
    entry.close();
  }
}

bool writeReadTest() {
  File f = SD.open(testFile, FILE_WRITE);
  if (!f) {
    Serial.println("open for write failed");
    return false;
  }
  f.print(testContent);
  f.close();

  f = SD.open(testFile, FILE_READ);
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
  Serial.println("starting SD (SPI)...");

  SPI.begin(kPinSdClk, kPinSdMiso, kPinSdMosi, kPinSdCs);
  if (!SD.begin(kPinSdCs)) {
    Serial.println("SD.begin() failed - check card is inserted");
    return;
  }

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("no SD card found");
    return;
  }
  Serial.printf("card type: %d, size: %llu MB\n", cardType,
                SD.cardSize() / (1024 * 1024));

  Serial.println(writeReadTest() ? "write/read test: OK"
                                  : "write/read test: FAILED");

  Serial.println("root directory:");
  listDir("/");
}

void loop() {}
