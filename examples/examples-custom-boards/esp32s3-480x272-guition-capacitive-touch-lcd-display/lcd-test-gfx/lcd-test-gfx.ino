/**
 * @file lcd-test-gfx.ino
 * @brief Alternative to lcd-test.ino for the Guition ESP32-S3 4.3"
 * 480x272 display (New Vision NV3041A over QSPI + GT911 capacitive
 * touch), using moononournation's "GFX Library for Arduino"
 * (https://github.com/moononournation/Arduino_GFX) for the display and
 * TAMC_GT911 for touch, instead of TinyGPU's NV3041ADriver/
 * TouchDriverGT911. Confirmed working on real hardware, including touch.
 *
 * TAMC_GT911::read() applies a coordinate transform for its default
 * ROTATION_NORMAL (x = width - x; y = height - y), which matches this
 * board's touch panel orientation as wired.
 *
 * Two tests, tap the screen to switch between them:
 *  - RGB test: red/green/blue bands.
 *  - Greyscale test: 20-band ramp from black to white.
 *
 * Dependencies:
 * - https://github.com/moononournation/Arduino_GFX ("GFX Library for
 *   Arduino" in Library Manager)
 * - TAMC_GT911 (Library Manager)
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include <Arduino_GFX_Library.h>
#include <TAMC_GT911.h>

// --- display geometry ----------------------------------------------------
constexpr int kDisplayWidth = 480;
constexpr int kDisplayHeight = 272;

// --- QSPI LCD pins (NV3041A, confirmed working) ---------------------------
constexpr int8_t kPinLcdCs = 45;
constexpr int8_t kPinLcdSclk = 47;
constexpr int8_t kPinLcdD0 = 21;
constexpr int8_t kPinLcdD1 = 48;
constexpr int8_t kPinLcdD2 = 40;
constexpr int8_t kPinLcdD3 = 39;
constexpr int kPinBacklight = 1;

// --- Touch I2C pins (GT911, confirmed working) -----------------------------
constexpr int8_t kPinTouchSda = 8;
constexpr int8_t kPinTouchScl = 4;
constexpr int8_t kPinTouchInt = 3;
constexpr int8_t kPinTouchRst = 38;

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    kPinLcdCs, kPinLcdSclk, kPinLcdD0, kPinLcdD1, kPinLcdD2, kPinLcdD3);
Arduino_GFX *gfx = new Arduino_NV3041A(bus, GFX_NOT_DEFINED /* RST */,
                                       0 /* rotation */, true /* IPS */);
TAMC_GT911 touch(kPinTouchSda, kPinTouchScl, kPinTouchInt, kPinTouchRst,
                 kDisplayWidth, kDisplayHeight);

void showRgbTest() {
  gfx->fillScreen(RGB565_BLACK);
  const int16_t bandHeight = kDisplayHeight / 3;
  const int16_t lastBandHeight = kDisplayHeight - 2 * bandHeight;

  gfx->fillRect(0, 0 * bandHeight, kDisplayWidth, bandHeight, RGB565_RED);
  gfx->fillRect(0, 1 * bandHeight, kDisplayWidth, bandHeight, RGB565_GREEN);
  gfx->fillRect(0, 2 * bandHeight, kDisplayWidth, lastBandHeight, RGB565_BLUE);

  gfx->setTextSize(2);
  gfx->setCursor(4, 0 * bandHeight + bandHeight / 2 - 8);
  gfx->setTextColor(RGB565_WHITE, RGB565_RED);
  gfx->print("RED (255,0,0)");
  gfx->setCursor(4, 1 * bandHeight + bandHeight / 2 - 8);
  gfx->setTextColor(RGB565_BLACK, RGB565_GREEN);
  gfx->print("GREEN (0,255,0)");
  gfx->setCursor(4, 2 * bandHeight + lastBandHeight / 2 - 8);
  gfx->setTextColor(RGB565_WHITE, RGB565_BLUE);
  gfx->print("BLUE (0,0,255)");

  Serial.println(
      "RGB test: top=RED(255,0,0) mid=GREEN(0,255,0) bottom=BLUE(0,0,255)");
}

void showGreyscaleTest() {
  gfx->fillScreen(RGB565_BLACK);
  constexpr int kBandCount = 20;
  const int16_t bandHeight = kDisplayHeight / kBandCount;

  Serial.println("Greyscale test: 20 bands, top=0 (black) to bottom=255 (white)");
  gfx->setTextSize(1);
  for (int i = 0; i < kBandCount; ++i) {
    const uint8_t value = static_cast<uint8_t>((255 * i) / (kBandCount - 1));
    const uint16_t color = RGB565(value, value, value);
    const uint16_t textColor = (value > 127) ? RGB565_BLACK : RGB565_WHITE;

    gfx->fillRect(0, i * bandHeight, kDisplayWidth, bandHeight, color);
    gfx->setCursor(4, i * bandHeight + bandHeight / 2 - 4);
    gfx->setTextColor(textColor, color);
    gfx->printf("%2d: %3d", i, value);

    // Serial.printf("  band %2d: grey %3d  packed 0x%04X\n", i, value, color);
  }
}

bool showingGreyscale = false;

void toggleTest() {
  showingGreyscale = !showingGreyscale;
  if (showingGreyscale) {
    showGreyscaleTest();
  } else {
    showRgbTest();
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(kPinBacklight, OUTPUT);
  digitalWrite(kPinBacklight, HIGH);

  if (!gfx->begin()) {
    Serial.println("gfx->begin() failed - check QSPI pins");
    while (true) delay(1000);
  }

  touch.begin();  // also calls Wire.begin(kPinTouchSda, kPinTouchScl)

  showRgbTest();
}

void loop() {
  static bool wasTouched = false;

  touch.read();
  const bool touched = touch.isTouched;

  if (touched) {
    Serial.printf("touch: x=%d y=%d size=%d\n", touch.points[0].x,
                  touch.points[0].y, touch.points[0].size);
  }
  if (touched && !wasTouched) {
    toggleTest();
  }
  wasTouched = touched;
  delay(20);
}
