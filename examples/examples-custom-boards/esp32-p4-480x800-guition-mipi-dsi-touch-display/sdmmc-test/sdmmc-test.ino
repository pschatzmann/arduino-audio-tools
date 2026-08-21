/**
 * @file sdmmc-test.ino
 * @brief MicroSD card test for the Guition JC4880P443C_I_W (ESP32-P4),
 * over SD_MMC in 4-bit mode.
 * *
 * This board's SD rail (TF_VCC) is powered from an on-chip LDO channel
 * (VO4 @ 3.3V), not a GPIO - per guition-jc4880p4-bsp's board_p4.c, the
 * card won't mount without it (a documented "0x107 timeout" gotcha).
 * That LDO is normally brought up as an undocumented side effect of
 * that library's board_p4_display_init() call, but ESP32-P4's SD_MMC
 * driver has its own direct mechanism for this
 * (`SOC_SDMMC_IO_POWER_EXTERNAL`, not present on ESP32-S3): calling
 * `SD_MMC.setPowerChannel(4)` before `begin()` acquires the same LDO
 * channel internally, so this file doesn't need to depend on the
 * display library at all.
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

// Pins from guition-jc4880p4-bsp's board_p4_pins.h "microSD" section.
constexpr int kPinSdClk = 43;
constexpr int kPinSdCmd = 44;
constexpr int kPinSdD0 = 39;
constexpr int kPinSdD1 = 40;
constexpr int kPinSdD2 = 41;
constexpr int kPinSdD3 = 42;
constexpr int kSdLdoChannel = 4;  // TF_VCC - see file header

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

#ifdef SOC_SDMMC_IO_POWER_EXTERNAL
  SD_MMC.setPowerChannel(kSdLdoChannel);
#else
  Serial.println(
      "warning: SOC_SDMMC_IO_POWER_EXTERNAL not defined for this target - "
      "TF_VCC LDO won't be powered, card likely won't mount");
#endif

  if (!SD_MMC.setPins(kPinSdClk, kPinSdCmd, kPinSdD0, kPinSdD1, kPinSdD2,
                      kPinSdD3)) {
    Serial.println("SD_MMC.setPins() failed");
    return;
  }
  if (!SD_MMC.begin("/sdcard", false)) {  // false = 4-bit mode
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
