/**
 * @file lcd-test.ino
 * @brief SpriteDisplay-based hardware color diagnostic for the Guition
 * ESP32-S3 4.3" 480x272 display (New Vision NV3041A over QSPI + GT911
 * capacitive touch), ported from the esp32-s3-240x320-lcd-display example
 * in this repo.
 *
 * This board went through two wrong assumptions before landing here: it
 * was first assumed to be an RGB-parallel ("DPI") panel (a guessed pin
 * table for that never got confirmed), until the user-supplied LCD pins
 * (CS/SCLK/D0-D3/Backlight, 2026-08-19) turned out to be a QSPI panel -
 * an entirely different interface (1 clock, 1 CS, 4 data lines, no
 * HSYNC/VSYNC/DE) - driven by an NV3041A controller. TinyGPU had no QSPI
 * display driver at all before this; see DisplayDriverQSPI.h's new
 * DisplayDriverQSPI base class and NV3041ADriver subclass.
 *
 * Two tests, tap the screen to switch between them:
 *  - RGB test: red/green/blue bands - reveals a panel that doesn't honor
 *    the RGB/BGR field wiring correctly.
 *  - Greyscale test: 20-band ramp from black to white - reveals a visible
 *    color tint in near-black tones even when RGB field wiring is correct.
 *
 * IMPORTANT: the LCD pins below are user-supplied/confirmed real
 * hardware pins, but NV3041ADriver's init register sequence is an
 * UNTESTED best-effort reconstruction (see DisplayDriverQSPI.h's header
 * comment on NV3041ADriver) - expect to need to debug/correct it.
 *
 * Touch pins (SDA=8/SCL=4/INT=3/RST=38) are user-confirmed (2026-08-19,
 * JC4827W543C), superseding an earlier guess (SDA=19/SCL=20, no RST/IRQ
 * wired) that left the touch chip unresponsive on I2C - with RST/IRQ
 * now wired, TouchDriverGT911::begin() runs GT911's documented
 * address-select reset sequence, which the earlier guess had no pins
 * for at all.
 *
 * Dependencies:
 * - https://github.com/pschatzmann/TinyGPU
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include <TinyGPU.h>
#include <TinyGPU/DisplayDriverQSPI.h>
#include <TinyGPU/SpriteDisplay.h>

using PixelT = RGB565;

// --- display geometry --------------------------------------------------
constexpr int kDisplayWidth = 480;
constexpr int kDisplayHeight = 272;

// --- QSPI LCD pins (NV3041A, user-supplied 2026-08-19) ------------------
constexpr int8_t kPinLcdCs = 45;
constexpr int8_t kPinLcdSclk = 47;
constexpr int8_t kPinLcdD0 = 21;
constexpr int8_t kPinLcdD1 = 48;
constexpr int8_t kPinLcdD2 = 40;
constexpr int8_t kPinLcdD3 = 39;
constexpr int kPinBacklight = 1;

// --- Touch I2C pins (GT911 - see lcd-test-gfx.ino for why SDA=19/SCL=20
// was tried and rejected: those are ESP32-S3's native USB D-/D+ pins) ---
constexpr int8_t kPinTouchSda = 8;
constexpr int8_t kPinTouchScl = 4;
constexpr int8_t kPinTouchInt = 3;
constexpr int8_t kPinTouchRst = 38;

NV3041ADriver<PixelT> tftDriver(kPinLcdCs, kPinLcdSclk, kPinLcdD0, kPinLcdD1,
                                kPinLcdD2, kPinLcdD3, kDisplayWidth,
                                kDisplayHeight);
SpriteDisplay<PixelT> display(kDisplayWidth, kDisplayHeight, tftDriver,
                              PixelT::fromRGB(0, 0, 0));
TouchDriverGT911 touchDriver(Wire, kPinTouchRst, kPinTouchInt);
BitmapFont<PixelT> font;

// Draws a solid band with a text label burned into it. Safe against the
// dangling-pointer trap of SpriteDisplay::addSprite(x, y, surface&) - the
// surface is heap-allocated and ownership is handed to SpriteDisplay via
// isSurfaceAutoDelete, the same pattern SpriteDisplay's own
// addSprite(x, y, maxX, maxY, color) overload uses internally.
void addLabeledBand(size_t x, size_t y, size_t w, size_t h, PixelT bgColor,
                    PixelT textColor, const char* label) {
  auto sprite = std::make_unique<Sprite<PixelT>>(w, h, font);
  sprite->begin();
  sprite->clear(bgColor);
  sprite->drawText(4, static_cast<int16_t>(h / 2 - 4), label, textColor, bgColor,
                   true);
  auto& info = display.addSprite(x, y, *sprite);
  info.isSurfaceAutoDelete = true;
  sprite.release();
}

void showRgbTest() {
  display.clear();
  const size_t bandHeight = kDisplayHeight / 3;
  const size_t lastBandHeight = kDisplayHeight - 2 * bandHeight;

  addLabeledBand(0, 0 * bandHeight, kDisplayWidth, bandHeight,
                PixelT::fromRGB(255, 0, 0), PixelT::fromRGB(255, 255, 255),
                "RED (255,0,0)");
  addLabeledBand(0, 1 * bandHeight, kDisplayWidth, bandHeight,
                PixelT::fromRGB(0, 255, 0), PixelT::fromRGB(0, 0, 0),
                "GREEN (0,255,0)");
  addLabeledBand(0, 2 * bandHeight, kDisplayWidth, lastBandHeight,
                PixelT::fromRGB(0, 0, 255), PixelT::fromRGB(255, 255, 255),
                "BLUE (0,0,255)");

  Serial.println(
      "RGB test: top=RED(255,0,0) mid=GREEN(0,255,0) bottom=BLUE(0,0,255)");
}

void showGreyscaleTest() {
  display.clear();
  constexpr int kBandCount = 20;
  const size_t bandHeight = kDisplayHeight / kBandCount;

  Serial.println("Greyscale test: 20 bands, top=0 (black) to bottom=255 (white)");
  for (int i = 0; i < kBandCount; ++i) {
    const uint8_t value = static_cast<uint8_t>((255 * i) / (kBandCount - 1));
    const PixelT color = PixelT::fromRGB(value, value, value);
    const PixelT textColor =
        (value > 127) ? PixelT::fromRGB(0, 0, 0) : PixelT::fromRGB(255, 255, 255);
    char label[16];
    snprintf(label, sizeof(label), "%2d: %3d", i, value);
    addLabeledBand(0, i * bandHeight, kDisplayWidth, bandHeight, color,
                  textColor, label);
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

  if (!display.begin()) {
    Serial.println("display.begin() failed - check QSPI pins/init sequence");
    while (true) delay(1000);
  }

  Wire.begin(kPinTouchSda, kPinTouchScl);
  delay(50);
  Serial.println("I2C scan (touch bus):");
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  found device at 0x%02X\n", addr);
    }
  }
  bool touchOk = touchDriver.begin();
  Serial.printf("touch begin(): %s\n", touchOk ? "OK" : "FAILED (no I2C ack from GT911)");
  display.setTouchDriver(touchDriver);

  showRgbTest();
}

void loop() {
  static bool wasTouched = false;
  static uint32_t lastHeartbeat = 0;
  const bool touched = touchDriver.isTouched();

  if (millis() - lastHeartbeat > 1000) {
    Serial.printf("heartbeat: isTouched()=%d\n", touched);
    lastHeartbeat = millis();
  }

  if (touched) {
    Point p;
    if (touchDriver.getPoint(p)) {
      Serial.printf("touch: x=%d y=%d pressure=%u\n", p.x, p.y, p.pressure);
    } else {
      Serial.println("touch: isTouched() true but getPoint() failed");
    }
  }
  if (touched && !wasTouched) {
    toggleTest();
  }
  wasTouched = touched;
  delay(30);
}
