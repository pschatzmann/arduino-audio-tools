/**
 * @file sd-test.ino
 * @brief MicroSD card test for the Guition ESP32-S3 4.3" 480x272 display,
 * accessed over plain SPI (SD.h) rather than SD_MMC. See sdmmc-test.ino
 * in the sibling folder for the same card over SD_MMC instead - it's the
 * same 4 physical pins either way (SD cards support both interfaces on
 * the same wiring: CLK, CMD/MOSI, DAT0/MISO, DAT3/CS), just accessed
 * through a different ESP32 peripheral/library. SD_MMC is what got
 * confirmed working first; this SPI variant is UNTESTED but should work
 * on the same wiring since it's electrically the same bus in a different
 * mode - verify on your hardware before relying on it.
 *
 * Writes a small test file, reads it back, and lists the card's root
 * directory.
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include <FS.h>
#include <SD.h>
#include <SPI.h>

// Same physical pins as sdmmc-test.ino, confirmed working there;
// untested in SPI mode - see file header.
constexpr int kPinSdCs = 10;   // DAT3 in SD_MMC mode
constexpr int kPinSdMosi = 11; // CMD in SD_MMC mode
constexpr int kPinSdClk = 12;
constexpr int kPinSdMiso = 13; // DAT0 in SD_MMC mode

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
