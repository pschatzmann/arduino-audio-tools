/**
 * @file lcd-test.ino
 * @brief SpriteDisplay-based hardware color diagnostic for the Hosyond 2.8"
 * ESP32-S3 Display (240x320 ILI9341 SPI panel + FT6336G capacitive touch),
 * ported from TinyGPU's color-test example
 * (https://github.com/pschatzmann/TinyGPU/blob/main/examples/color-test/color-test.ino)
 * with this board's actual pins.
 *
 * Two tests, tap the screen to switch between them:
 *  - RGB test: red/green/blue bands - reveals a panel that doesn't honor
 *    the RGB/BGR field wiring correctly (e.g. the "green" band showing up
 *    blue).
 *  - Greyscale test: 20-band ramp from black to white - reveals a visible
 *    color tint in near-black tones even when RGB field wiring is correct.
 * Serial also prints a description of each band, in case a tint or swap
 * makes a band ambiguous by eye alone.
 *
 * Dependencies:
 * - https://github.com/pschatzmann/TinyGPU
 * @see https://www.lcdwiki.com/2.8inch_ESP32-S3_Display
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include <TinyGPU.h>
#include <TinyGPU/DisplayDriverSPI.h>
#include <TinyGPU/SpriteDisplay.h>

using PixelT = RGB565;

// --- display geometry ------------------------------------------------------
constexpr int kDisplayWidth = 240;
constexpr int kDisplayHeight = 320;

// --- SPI / display pins (ILI9341) ------------------------------------------
constexpr int8_t kPinMosi = 11;
constexpr int8_t kPinMiso = 13;
constexpr int8_t kPinSclk = 12;
constexpr int8_t kPinCs = 10;
constexpr int8_t kPinDc = 46;
constexpr int8_t kPinRst = -1;  // shared with the board's EN/reset line
constexpr int8_t kPinBacklight = 45;

// --- Touch I2C pins (FT6336G, shares the codec's I2C bus) ------------------
constexpr int8_t kPinTouchSda = 16;
constexpr int8_t kPinTouchScl = 15;
constexpr int8_t kPinTouchRst = 18;
constexpr int8_t kPinTouchIrq = 17;

ILI9341Driver<PixelT> tftDriver(SPI, kPinCs, kPinDc, kPinRst);
SpriteDisplay<PixelT> display(kDisplayWidth, kDisplayHeight, tftDriver,
                              PixelT::fromRGB(0, 0, 0));
// IRQ left disabled (-1): TouchDriverFT6236::isTouched() would otherwise
// skip the I2C read whenever the IRQ pin reads HIGH, and this board's INT
// line's actual idle polarity/mode hasn't been confirmed - polling the
// status register directly is simpler and costs nothing.
//
// NOTE: on real hardware (2026-08-18), the FT6336G ACKs on I2C (0x38) and
// begin() succeeds, but TD_STATUS stays 0x00 even while holding a tap -
// confirmed via the raw register dump in dumpTouchRegisters() below, which
// bypasses this driver entirely, so it isn't a driver/register-map bug.
// Points at the touch panel's FPC connector not being seated - re-test
// once that's been checked.
TouchDriverFT6236 touchDriver(Wire, kPinTouchRst, -1);
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
    Serial.printf("  band %2d: grey %3d  packed 0x%04X\n", i, value,
                  color.getValue());
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

  SPI.begin(kPinSclk, kPinMiso, kPinMosi, kPinCs);
  display.begin();
  // This panel's color filter needs the ILI9341 inversion bit set, or every
  // color comes out as its photographic negative (RED<->CYAN, GREEN<->
  // MAGENTA, BLUE<->YELLOW) - confirmed on real hardware.
  tftDriver.setInvertColor(true);

  Wire.begin(kPinTouchSda, kPinTouchScl);
  delay(50);
  Serial.println("I2C scan (touch bus, SDA=16/SCL=15):");
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  found device at 0x%02X\n", addr);
    }
  }
  bool touchOk = touchDriver.begin();
  Serial.printf("touch begin(): %s\n", touchOk ? "OK" : "FAILED (no I2C ack from FT6336G at 0x38)");
  display.setTouchDriver(touchDriver);

  showRgbTest();
}

// Raw register dump, bypassing TouchDriverFT6236 entirely, to tell apart
// "the chip really reports 0 touches" from "something in the driver's I2C
// read is silently failing" - registers 0x00 (DEVICE_MODE), 0x02
// (TD_STATUS), 0x03-0x06 (P1 X/Y).
void dumpTouchRegisters() {
  Wire.beginTransmission(0x38);
  Wire.write((uint8_t)0x00);
  if (Wire.endTransmission(false) != 0) {
    Serial.println("  raw: endTransmission failed");
    return;
  }
  uint8_t n = Wire.requestFrom(0x38, 7);
  if (n != 7) {
    Serial.printf("  raw: requestFrom got %d/7 bytes\n", n);
    return;
  }
  uint8_t buf[7];
  for (int i = 0; i < 7; i++) buf[i] = Wire.read();
  Serial.printf("  raw: MODE=%02X GEST=%02X STATUS=%02X P1_XH=%02X P1_XL=%02X P1_YH=%02X P1_YL=%02X\n",
                buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6]);
}

void loop() {
  static bool wasTouched = false;
  static uint32_t lastHeartbeat = 0;
  const bool touched = touchDriver.isTouched();

  if (millis() - lastHeartbeat > 1000) {
    Serial.printf("heartbeat: isTouched()=%d\n", touched);
    dumpTouchRegisters();
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
