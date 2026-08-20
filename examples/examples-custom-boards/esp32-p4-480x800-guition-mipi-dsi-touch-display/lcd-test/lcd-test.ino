/**
 * @file lcd-test.ino
 * @brief SpriteDisplay-based hardware color diagnostic for the Guition
 * JC4880P443C_I_W (ESP32-P4, ST7701S 480x800 MIPI-DSI panel + GT911
 * capacitive touch), following the same RGB-test/greyscale-test pattern
 * as this repo's other lcd-test.ino examples.
 *
 * Two tests, tap the screen to switch between them:
 *  - RGB test: red/green/blue bands - reveals a panel that doesn't honor
 *    the RGB/BGR field wiring correctly.
 *  - Greyscale test: 20-band ramp from black to white - reveals a visible
 *    color tint in near-black tones even when RGB field wiring is correct.
 *
 * This board's ST7701S MIPI-DSI panel needed a new TinyGPU driver
 * (DisplayDriverDSI.h's ST7701Driver) - see that file for the panel's
 * init sequence and its guition-jc4880p4-bsp attribution. Touch needed no
 * new code: GT911 is plain I2C, so TinyGPU's existing TouchDriverGT911
 * works unchanged.
 *
 * UNTESTED - written without access to this hardware, same as every other
 * example in this board folder. This board's ESP32-P4 is also an
 * engineering-sample chip (rev v1.3) that guition-jc4880p4-bsp documents
 * as needing a pinned PlatformIO toolchain or it hits "Illegal
 * instruction" at boot - see that BSP's README if this doesn't boot.
 *
 * Dependencies:
 * - https://github.com/pschatzmann/TinyGPU
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include <TinyGPU.h>
#include <TinyGPU/DisplayDriverDSI.h>
#include <TinyGPU/SpriteDisplay.h>

using PixelT = RGB565;

// --- display geometry (native portrait) --------------------------------
constexpr int kDisplayWidth = 480;
constexpr int kDisplayHeight = 800;

// --- Display pins/config (ST7701S MIPI-DSI), from
// guition-jc4880p4-bsp's board_p4_pins.h/board_p4.c ---------------------
constexpr int8_t kPinLcdRst = 5;
constexpr int8_t kPinLcdBacklight = 23;
constexpr int8_t kDsiPhyLdoChan = 3;
constexpr uint16_t kDsiPhyLdoMv = 2500;

// --- Touch I2C pins (GT911), same source ---------------------------------
constexpr int8_t kPinTouchSda = 7;
constexpr int8_t kPinTouchScl = 8;
constexpr int8_t kPinTouchRst = 3;
constexpr int8_t kPinTouchInt = -1;  // unused/polled on this board

ST7701Driver<PixelT> tftDriver(kPinLcdRst, kPinLcdBacklight, kDisplayWidth,
                               kDisplayHeight, kDsiPhyLdoChan, kDsiPhyLdoMv);
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

  if (!display.begin()) {
    Serial.println("display.begin() failed - check DSI bus/init sequence");
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
