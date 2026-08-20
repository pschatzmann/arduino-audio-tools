/**
 * @file lcd-test.ino
 * @brief Display + touch diagnostic for the Guition JC4880P443C_I_W
 * (ESP32-P4, ST7701S 480x800 MIPI-DSI panel + GT911 capacitive touch).
 *
 * UNTESTED - written without access to this hardware. The display/touch
 * bring-up itself is not this file's code: it's entirely delegated to
 * the guition-jc4880p4-bsp library (MIT, https://github.com/ultramcu/
 * guition-jc4880p4-bsp), which does a from-scratch ST7701S MIPI-DSI init
 * (no esp_lcd_st7701 vendor component) plus GT911 touch bring-up. This
 * file only calls that library's public API (board_p4.h) and draws a
 * simple test pattern.
 *
 * IMPORTANT: the BSP's own example comments flag that this board's
 * ESP32-P4 is an ENGINEERING-SAMPLE chip (rev v1.3) that needs
 * `"chip_variant": "esp32p4_es"` and a specific pinned toolchain
 * (pioarduino 55.03.36-1) under PlatformIO, or it hits "Illegal
 * instruction" at the 2nd-stage bootloader. This example instead uses
 * arduino-cli's generic `esp32:esp32:esp32p4` target, which has not
 * been confirmed to handle the engineering-sample silicon the same way
 * - if it doesn't boot, that pinned-toolchain requirement is the first
 * thing to check. See the BSP's README for the full story.
 *
 * Draws 8 vertical color bars, then polls touch and prints native
 * (480x800 portrait) coordinates over Serial.
 *
 * Dependencies:
 * - https://github.com/ultramcu/guition-jc4880p4-bsp (this repo's
 *   `library.properties` names `esp_lcd_touch_gt911` as a dependency,
 *   but that's not a registered Arduino Library Manager entry - it
 *   needs to be vendored manually. See this board folder's README.)
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include <Arduino.h>
#include "board_p4.h"

// 8 vertical color bars across the 480-wide native panel, RGB565.
constexpr uint16_t kBarColors[8] = {
    0xF800,  // red
    0x07E0,  // green
    0x001F,  // blue
    0xFFE0,  // yellow
    0x07FF,  // cyan
    0xF81F,  // magenta
    0xFFFF,  // white
    0x0000,  // black
};

void drawColorBars() {
  uint16_t* fb = board_p4_get_framebuffer();
  if (fb == nullptr) {
    Serial.println("framebuffer is NULL - display init failed?");
    return;
  }

  constexpr int w = BOARD_P4_LCD_H_RES;  // 480
  constexpr int h = BOARD_P4_LCD_V_RES;  // 800
  constexpr int barWidth = w / 8;

  for (int y = 0; y < h; ++y) {
    uint16_t* row = fb + static_cast<size_t>(y) * w;
    for (int x = 0; x < w; ++x) {
      int bar = x / barWidth;
      if (bar > 7) bar = 7;  // clamp the remainder column
      row[x] = kBarColors[bar];
    }
  }

  board_p4_flush_region(0, 0, w, h, fb);
  board_p4_present();
  Serial.println("color bars drawn");
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("Guition JC4880P443 (ESP32-P4) lcd-test");

  esp_err_t err = board_p4_display_init();
  if (err != ESP_OK) {
    Serial.printf("board_p4_display_init failed: %d\n", static_cast<int>(err));
  }
  board_p4_set_brightness(200);

  err = board_p4_touch_init();
  if (err != ESP_OK) {
    Serial.printf("board_p4_touch_init failed: %d\n", static_cast<int>(err));
  }

  drawColorBars();
  Serial.println("setup done - touch the screen");
}

void loop() {
  static uint32_t lastPrintMs = 0;

  int x = 0, y = 0;
  bool pressed = false;
  board_p4_touch_read(&x, &y, &pressed);

  if (pressed) {
    const uint32_t now = millis();
    if (now - lastPrintMs >= 100) {  // throttle to ~10Hz
      Serial.printf("touch: x=%d y=%d\n", x, y);
      lastPrintMs = now;
    }
  }

  delay(10);
}
