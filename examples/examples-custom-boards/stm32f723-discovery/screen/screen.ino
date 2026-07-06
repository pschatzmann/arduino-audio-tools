//
// Screen Example for STM32F723E Discovery Board
// The STM32F723E-Discovery has no IMU (unlike the STM32F411 Discovery), but
// it does have an on-board 1.54" 240x240 TFT (ST7789H2 controller).
//
// The panel is not attached to a "nice" peripheral like SPI - it is memory
// mapped through the FMC (NOR/SRAM Bank 2), 16 bit wide, with one address
// line acting as the command/data select line. None of the popular Arduino
// display libraries (Arduino_GFX, TFT_eSPI, ...) drive this exact bus/board
// out of the box, so the low-level bring-up below ports the relevant parts
// of ST's stm32f723e_discovery_lcd.c / stm32f723e_discovery.c BSP
// (STM32CubeF7 package) directly to Arduino/HAL calls.
//
// What we DON'T have to write ourselves: fonts, text layout, circles,
// lines, etc. Adafruit_GFX only requires a subclass to implement
// drawPixel() (and, for speed, fillRect()) - everything else (print(),
// setCursor(), drawCircle(), drawLine(), ...) comes from the library.
// Install "Adafruit GFX Library" via the Library Manager before compiling.
//
// Pins (see variant_DISCO_F723IE.h / the ST BSP):
//  - LCD_RESET:           PH7  (manual GPIO)
//  - LCD_BL (backlight):  PH11 (manual GPIO)
//  - LCD_TE (tearing fx): PC8  (input, unused here)
//  - FMC data/address bus + NE2 (chip select): GPIOD/E/F/G, AF12, see
//    fmcGpioInit() below
//

#include <Arduino.h>
#include <Adafruit_GFX.h>

extern "C" {
#include "stm32f7xx_hal.h"
}

#define LCD_RESET_PIN PH7
#define LCD_BL_PIN PH11

#define LCD_WIDTH 240
#define LCD_HEIGHT 240

// FMC Bank2 is memory mapped at this address. Writing to the first 16 bit
// half-word (REG) sends a command/index, writing to the second (RAM) sends
// data - this split is wired to the panel's D/C pin via an FMC address line,
// exactly like the ST BSP driver (see LCD_CONTROLLER_TypeDef there).
struct LcdBank2 {
  __IO uint16_t REG;
  __IO uint16_t RAM;
};
#define LCD_BANK2 ((LcdBank2 *)0x64000000UL)

// ST7789 commands used here
#define ST7789_SWRESET 0x01
#define ST7789_SLPOUT 0x11
#define ST7789_NORON 0x13
#define ST7789_INVON 0x21
#define ST7789_DISPON 0x29
#define ST7789_CASET 0x2A
#define ST7789_RASET 0x2B
#define ST7789_RAMWR 0x2C
#define ST7789_MADCTL 0x36
#define ST7789_COLMOD 0x3A

/**
 * @brief Adafruit_GFX driver for the ST7789H2 panel on the F723-Discovery's
 * FMC Bank2 bus. Implementing drawPixel()/fillRect() is all that's needed -
 * text, fonts, circles, lines etc. all come from Adafruit_GFX for free.
 */
class ST7789FmcGFX : public Adafruit_GFX {
 public:
  ST7789FmcGFX() : Adafruit_GFX(LCD_WIDTH, LCD_HEIGHT) {}

  void begin() {
    // Note: the FMC Bank2 (LCD) address window must be marked non-cacheable
    // in the MPU before anything below touches it, or writes get absorbed
    // into the Cortex-M7 D-Cache instead of reaching the panel (no crash,
    // no hang, the panel just stays blank). That's handled once for the
    // whole board in initVariant() (variant_DISCO_F723IE.cpp in the STM32
    // core), not here, so every sketch on this board gets it for free.
    Serial.println("fmcBank2Init...");
    fmcBank2Init();
    Serial.println("st7789Init...");
    st7789Init();
    Serial.println("begin done.");
  }

  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if (x < 0 || y < 0 || x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    setAddrWindow(x, y, x, y);
    lcdWriteData16(color);
  }

  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                uint16_t color) override {
    if (w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > LCD_WIDTH) w = LCD_WIDTH - x;
    if (y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    setAddrWindow(x, y, x + w - 1, y + h - 1);
    uint32_t count = (uint32_t)w * h;
    for (uint32_t i = 0; i < count; i++) lcdWriteData16(color);
  }

 private:
  static inline void lcdWriteCommand(uint8_t cmd) {
    LCD_BANK2->REG = cmd;
    __DSB();
  }

  static inline void lcdWriteData16(uint16_t data) {
    LCD_BANK2->RAM = data;
    __DSB();
  }

  static void lcdWriteCommandData8(uint8_t cmd, uint8_t data) {
    lcdWriteCommand(cmd);
    lcdWriteData16(data);
  }

  static void setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1,
                             uint16_t y1) {
    lcdWriteCommand(ST7789_CASET);
    lcdWriteData16(x0 >> 8);
    lcdWriteData16(x0 & 0xFF);
    lcdWriteData16(x1 >> 8);
    lcdWriteData16(x1 & 0xFF);

    lcdWriteCommand(ST7789_RASET);
    lcdWriteData16(y0 >> 8);
    lcdWriteData16(y0 & 0xFF);
    lcdWriteData16(y1 >> 8);
    lcdWriteData16(y1 & 0xFF);

    lcdWriteCommand(ST7789_RAMWR);
  }

  // ---- FMC Bank2 bring-up (ports the ST BSP FMC_BANK2_Init/MspInit) ----

  static void fmcGpioInit() {
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF12_FMC;

    // LCD/PSRAM D2,D3,NOE,NWE,PSRAM_NE1,D13,D14,D15,PSRAM_A16,A17,D0,D1
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_7 |
               GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 |
               GPIO_PIN_12 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOD, &gpio);

    // PSRAM_NBL0/1, D4..D12
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 |
               GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 |
               GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOE, &gpio);

    // PSRAM_A0..A9
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 |
               GPIO_PIN_5 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 |
               GPIO_PIN_15;
    HAL_GPIO_Init(GPIOF, &gpio);

    // PSRAM_A10..A15, LCD_NE (bank2 chip select)
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 |
               GPIO_PIN_5 | GPIO_PIN_9;
    HAL_GPIO_Init(GPIOG, &gpio);
  }

  static void fmcBank2Init() {
    __HAL_RCC_FMC_CLK_ENABLE();
    __HAL_RCC_FMC_FORCE_RESET();
    __HAL_RCC_FMC_RELEASE_RESET();

    fmcGpioInit();

    static SRAM_HandleTypeDef hsram;
    FMC_NORSRAM_TimingTypeDef timing = {0};

    hsram.Instance = FMC_NORSRAM_DEVICE;
    hsram.Extended = FMC_NORSRAM_EXTENDED_DEVICE;
    hsram.Init.NSBank = FMC_NORSRAM_BANK2;
    hsram.Init.DataAddressMux = FMC_DATA_ADDRESS_MUX_DISABLE;
    hsram.Init.MemoryType = FMC_MEMORY_TYPE_SRAM;
    hsram.Init.MemoryDataWidth = FMC_NORSRAM_MEM_BUS_WIDTH_16;
    hsram.Init.BurstAccessMode = FMC_BURST_ACCESS_MODE_DISABLE;
    hsram.Init.WaitSignalPolarity = FMC_WAIT_SIGNAL_POLARITY_LOW;
    hsram.Init.WaitSignalActive = FMC_WAIT_TIMING_BEFORE_WS;
    hsram.Init.WriteOperation = FMC_WRITE_OPERATION_ENABLE;
    hsram.Init.WaitSignal = FMC_WAIT_SIGNAL_DISABLE;
    hsram.Init.ExtendedMode = FMC_EXTENDED_MODE_DISABLE;
    hsram.Init.AsynchronousWait = FMC_ASYNCHRONOUS_WAIT_DISABLE;
    hsram.Init.WriteBurst = FMC_WRITE_BURST_DISABLE;
    hsram.Init.WriteFifo = FMC_WRITE_FIFO_DISABLE;
    hsram.Init.PageSize = FMC_PAGE_SIZE_NONE;
    hsram.Init.ContinuousClock = FMC_CONTINUOUS_CLOCK_SYNC_ONLY;

    // Timing derived for ~108MHz FMC clock, same as the ST BSP reference
    timing.AddressSetupTime = 9;
    timing.AddressHoldTime = 2;
    timing.DataSetupTime = 6;
    timing.BusTurnAroundDuration = 1;
    timing.CLKDivision = 2;
    timing.DataLatency = 2;
    timing.AccessMode = FMC_ACCESS_MODE_A;

    HAL_SRAM_Init(&hsram, &timing, &timing);
  }

  static void st7789Init() {
    pinMode(LCD_RESET_PIN, OUTPUT);
    pinMode(LCD_BL_PIN, OUTPUT);

    digitalWrite(LCD_BL_PIN, HIGH);  // backlight on
    Serial.println("  backlight pin set HIGH");

    digitalWrite(LCD_RESET_PIN, LOW);
    delay(5);
    digitalWrite(LCD_RESET_PIN, HIGH);
    delay(10);
    digitalWrite(LCD_RESET_PIN, LOW);
    delay(20);
    digitalWrite(LCD_RESET_PIN, HIGH);
    delay(10);
    Serial.println("  reset pulse done");

    lcdWriteCommand(ST7789_SWRESET);
    delay(150);
    lcdWriteCommand(ST7789_SLPOUT);
    delay(120);
    lcdWriteCommandData8(ST7789_COLMOD, 0x55);  // 16 bit/pixel (RGB565)
    delay(10);
    // Orientation/RGB-BGR order: adjust bit 0x08 here if colors/axes look
    // swapped on your panel revision - the exact value isn't in the public
    // BSP header (it lives in the closed ST7789H2 component driver).
    lcdWriteCommandData8(ST7789_MADCTL, 0x00);
    lcdWriteCommand(ST7789_INVON);  // most ST7789 panels need inversion on
    lcdWriteCommand(ST7789_NORON);
    delay(10);
    lcdWriteCommand(ST7789_DISPON);
    delay(120);
    Serial.println("  panel init commands sent");
  }
};

static uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

ST7789FmcGFX tft;

static void drawTestPattern() {
  tft.fillScreen(color565(0, 0, 0));

  // color bars
  const uint16_t colors[] = {
      color565(255, 0, 0),   color565(0, 255, 0),  color565(0, 0, 255),
      color565(255, 255, 0), color565(0, 255, 255), color565(255, 0, 255),
      color565(255, 255, 255)};
  const int n = sizeof(colors) / sizeof(colors[0]);
  int barHeight = LCD_HEIGHT / 5;
  for (int i = 0; i < n; i++) {
    tft.fillRect(i * LCD_WIDTH / n, 0, LCD_WIDTH / n + 1, barHeight,
                 colors[i]);
  }

  // frame
  tft.drawRect(0, 0, LCD_WIDTH, LCD_HEIGHT, color565(255, 255, 255));

  // radiating lines + circles from the center of the remaining area
  int16_t cx = LCD_WIDTH / 2;
  int16_t cy = barHeight + (LCD_HEIGHT - barHeight) / 2;
  for (int a = 0; a < 360; a += 30) {
    float rad = a * PI / 180.0f;
    int16_t x1 = cx + cos(rad) * 55;
    int16_t y1 = cy + sin(rad) * 55;
    tft.drawLine(cx, cy, x1, y1, color565(0, 200, 255));
  }
  tft.drawCircle(cx, cy, 40, color565(255, 200, 0));

  // text - this is all Adafruit_GFX, no font work of our own
  tft.setTextColor(color565(255, 255, 255));
  tft.setTextSize(2);
  tft.setCursor(20, barHeight + 10);
  tft.print("AudioTools");
  tft.setTextSize(1);
  tft.setCursor(20, barHeight + 32);
  tft.print("STM32F723 Discovery");
}

int16_t ballX = LCD_WIDTH / 2, ballY = LCD_HEIGHT / 2, ballDX = 3, ballDY = 2;
const int16_t ballR = 10;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(100);
  Serial.println("STM32F723 Discovery - screen example");

  tft.begin();

  drawTestPattern();
  delay(3000);
  tft.fillScreen(color565(0, 0, 0));
}

void loop() {
  static uint16_t ballColor = color565(255, 80, 80);

  // erase old position, move, redraw
  tft.fillCircle(ballX, ballY, ballR, color565(0, 0, 0));

  ballX += ballDX;
  ballY += ballDY;
  if (ballX - ballR <= 0 || ballX + ballR >= LCD_WIDTH) ballDX = -ballDX;
  if (ballY - ballR <= 0 || ballY + ballR >= LCD_HEIGHT) ballDY = -ballDY;

  tft.fillCircle(ballX, ballY, ballR, ballColor);
  delay(20);
}
