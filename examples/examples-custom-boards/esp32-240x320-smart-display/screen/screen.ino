/**
 * @file screen.ino
 * @brief ILI9341 TFT (240x320) color test for the ESP32 Arduino LVGL
 * WiFi/Bluetooth Development Board with 2.4" LCD TFT module
 * @see
 * https://github.com/pschatzmann/arduino-audio-tools/wiki/Audio-Boards#esp32-arduino-lvgl-wifibluetooth-development-board-24inch-lcd-tft-module
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SPI.h>

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST -1
#define TFT_BL 27

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Starting LCD test...");

  // Turn on backlight
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // IMPORTANT: use the board's SPI pins
  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);

  tft.begin();

  tft.setRotation(1);

  tft.fillScreen(ILI9341_BLACK);
  delay(500);

  tft.fillScreen(ILI9341_RED);
  delay(1000);

  tft.fillScreen(ILI9341_GREEN);
  delay(1000);

  tft.fillScreen(ILI9341_BLUE);
  delay(1000);

  tft.fillScreen(ILI9341_BLACK);

  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);

  tft.setCursor(20, 30);
  tft.println("ESP32 LCD TEST");

  tft.setCursor(20, 70);
  tft.println("ILI9341");

  tft.setCursor(20, 110);
  tft.println("240 x 320");

  Serial.println("LCD test finished.");
}

void loop() {}