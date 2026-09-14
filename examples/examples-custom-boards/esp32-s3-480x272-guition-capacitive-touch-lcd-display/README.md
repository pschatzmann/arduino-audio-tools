## Guition ESP32-S3 4.3" 480x272 Capacitive Touch Display

Sold under several names (Guition JC4827W543, Sunton ESP32-8048S043, and
similar).

## Hardware

-   **MCU:** ESP32-S3, 8MB PSRAM
-   **Display:** 4.3", 480x272, New Vision NV3041A over QSPI (1 clock, 1
    CS, 4 data lines, no HSYNC/VSYNC/DE)
-   **Touch:** GT911 capacitive, I2C
-   **Audio:** NS4168 I2S speaker amp only - no codec, no onboard mic

## Pins

| Function            | Pins |
| ------------------- | ---- |
| QSPI LCD (NV3041A)  | CS=45, SCLK=47, D0=21, D1=48, D2=40, D3=39, Backlight=1. Clock 32MHz. |
| Touch I2C (GT911)   | SDA=8, SCL=4, INT=3, RST=38, address 0x5D |
| Audio I2S (NS4168)  | BCK=42, WS=2, DATA=41 |
| SD (SD_MMC, 1-bit)  | CLK=12, CMD=11, D0=13. DAT3 (GPIO10) is wired but unused in code. |
| SD (SPI, alternative) | CS=10, MOSI=11, CLK=12, MISO=13 - same physical pins as SD_MMC above, untested |

## Build options

The `lcd-test`/`lcd-test-gfx` examples require:

-   `PSRAM=opi` - the board's default FQBN has PSRAM disabled
-   `CDCOnBoot=cdc` - required for `Serial` output over the native USB
    port; without it `Serial` routes to the UART0 pins instead

Example FQBN: `esp32:esp32:esp32s3:CDCOnBoot=cdc,PSRAM=opi`

## Examples

-   `lcd-test-gfx` - RGB/greyscale color diagnostic, tap to switch
    tests. Uses [Arduino_GFX](https://github.com/moononournation/Arduino_GFX)'s
    `Arduino_NV3041A` driver for the display and `TAMC_GT911` for touch.
-   `lcd-test` - same diagnostic, built on
    [TinyGPU](https://github.com/pschatzmann/TinyGPU)'s `NV3041ADriver`
    and `TouchDriverGT911` instead.
-   `audio-out` - sine wave playback through the NS4168 amp via
    `I2SStream`.
-   `sdmmc-test` - brings up the microSD card over `SD_MMC` in 1-bit
    mode, writes/reads a test file, lists the root directory.
-   `sd-test` - the same card over plain SPI (`SD.h`) instead of
    SD_MMC.
-   `player-sdmmc` - MP3 playback off the microSD card
    (`AudioSourceSDMMC` + `MP3DecoderHelix`) through the NS4168 amp.

Not included: `audio-in` (no onboard mic) and `led-test` (no RGB LED on
this board).

## TinyGPU Support

This board's QSPI LCD and GT911 touch controller are supported in
TinyGPU via:

-   `TinyGPU/DisplayDriverQSPI.h` - `DisplayDriverQSPI<RGB_T>`, a base
    class for QSPI TFT/AMOLED controllers (NV3041A, ST77916, CO5300,
    SH8601, AXS15231B, ...) wrapping ESP-IDF's
    `esp_lcd_new_panel_io_spi()` (quad mode). `NV3041ADriver` is the
    concrete NV3041A subclass.
-   `TinyGPU/TouchDriver.h` - `TouchDriverGT911`. Reports up to 5
    simultaneous touches; `getSecondPoint()` is a real second contact,
    not a stub.

## Dependencies

-   [arduino-audio-tools](https://github.com/pschatzmann/arduino-audio-tools)
    (`audio-out`, `player-sdmmc`)
-   [TinyGPU](https://github.com/pschatzmann/TinyGPU) (`lcd-test`)
-   [Arduino_GFX](https://github.com/moononournation/Arduino_GFX) ("GFX
    Library for Arduino" in Library Manager) (`lcd-test-gfx`)
-   [TAMC_GT911](https://github.com/TAMCTec/gt911-arduino) (Library
    Manager) (`lcd-test-gfx`)
