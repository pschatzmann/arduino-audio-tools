/**
 * @file sd.ino
 * @brief SD card (SPI) directory listing example for the ESP32 Arduino LVGL
 * WiFi/Bluetooth Development Board with 2.4" LCD TFT module
 * @see
 * https://github.com/pschatzmann/arduino-audio-tools/wiki/Audio-Boards#esp32-arduino-lvgl-wifibluetooth-development-board-24inch-lcd-tft-module
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include <SPI.h>
#include <SD.h>

constexpr int8_t SD_SCK = 18;
constexpr int8_t SD_MISO = 19;
constexpr int8_t SD_MOSI = 23;
constexpr int8_t SD_CS = 5;

SPIClass sdSPI(VSPI);

void list() {
  File root = SD.open("/");
  File entry;
  while ((entry = root.openNextFile())) {
    Serial.printf("  %s  (%u bytes)\n", entry.name(), entry.size());
    entry.close();
  }
}

void setup() {
  Serial.begin(115200);

  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, sdSPI)) {
    Serial.println("SD.begin() failed - check CS pin and wiring.");
    return;
  }

  Serial.printf("SD card type: %d, size: %llu MB\n", SD.cardType(),
               SD.cardSize() / (1024 * 1024));

}

void loop() {
  Serial.println("-------------------");
  list();
  Serial.println();
  delay(10000);
}
