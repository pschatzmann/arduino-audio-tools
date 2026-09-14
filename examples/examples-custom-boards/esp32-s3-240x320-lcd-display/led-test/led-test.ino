/**
 * @file led-test.ino
 * @brief Test for the single WS2812-compatible RGB LED on the Hosyond 2.8"
 * ESP32-S3 Display (IO42, single-wire). Cycles red/green/blue/white/off so
 * you can confirm the LED and its color channel wiring are all good.
 *
 * Dependencies:
 * - https://github.com/adafruit/Adafruit_NeoPixel
 * @see https://www.lcdwiki.com/2.8inch_ESP32-S3_Display
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include <Adafruit_NeoPixel.h>

constexpr int kPinLed = 42;
constexpr int kNumPixels = 1;

Adafruit_NeoPixel pixel(kNumPixels, kPinLed, NEO_GRB + NEO_KHZ800);

struct NamedColor {
  const char* name;
  uint8_t r, g, b;
};

const NamedColor colors[] = {
    {"RED", 255, 0, 0},   {"GREEN", 0, 255, 0}, {"BLUE", 0, 0, 255},
    {"WHITE", 255, 255, 255}, {"OFF", 0, 0, 0},
};

void setup() {
  Serial.begin(115200);
  Serial.println("starting RGB LED test...");

  pixel.begin();
  pixel.setBrightness(64);  // keep it comfortable to look at directly
}

void loop() {
  for (const auto& c : colors) {
    Serial.printf("LED: %s (%d,%d,%d)\n", c.name, c.r, c.g, c.b);
    pixel.setPixelColor(0, pixel.Color(c.r, c.g, c.b));
    pixel.show();
    delay(1000);
  }
}
