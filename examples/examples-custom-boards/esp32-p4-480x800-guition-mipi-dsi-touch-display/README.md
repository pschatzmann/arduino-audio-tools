## Guition JC4880P443C_I_W (ESP32-P4, 4.3" MIPI-DSI Touch Display)

**Status: UNTESTED.** Written without access to this hardware - all
six examples compile clean against arduino-cli's `esp32:esp32:esp32p4`
target, but none has been flashed to a real board. See "Known risk"
below before relying on this.

## Hardware

-   **MCU:** ESP32-P4 (dual-core RISC-V @ 400MHz) + ESP32-C6 Wi-Fi/BT
    co-processor over SDIO (ESP-Hosted)
-   **Display:** 4.3", 480x800, ST7701S over MIPI-DSI (2-lane)
-   **Touch:** GT911 capacitive, I2C
-   **Audio:** ES8311 codec + NS4150 speaker amp
-   **Storage:** microSD via SDMMC (4-bit), separate bus from the C6's
    Wi-Fi SDIO
-   **Power:** IP5306 Li-ion charger (single-cell, connector CN4)
-   32MB QSPI PSRAM, 16MB QIO flash

## Known risk

This board's ESP32-P4 is an **engineering-sample chip (rev v1.3)**. The
[guition-jc4880p4-bsp](https://github.com/ultramcu/guition-jc4880p4-bsp)
library that provides display/touch bring-up documents that under
PlatformIO this requires `"chip_variant": "esp32p4_es"` and a pinned
toolchain (`pioarduino 55.03.36-1`), or the board hits "Illegal
instruction" at the 2nd-stage bootloader. These examples instead
target arduino-cli's generic `esp32:esp32:esp32p4` board - both compile
clean against it, but whether that target's toolchain handles this
specific engineering-sample silicon the same way is unconfirmed. If a
board doesn't boot, this is the first thing to check.

## Pins

Source: [board_p4_pins.h](https://github.com/ultramcu/guition-jc4880p4-bsp/blob/main/src/board_p4_pins.h)
in guition-jc4880p4-bsp - documented there as "on-hardware VERIFIED" for
display/touch/SD/Wi-Fi; the audio pins are board-documented but "not
driven by the core BSP" (i.e. not exercised by that library).

| Function             | Pins |
| --------------------- | ---- |
| Display (ST7701S MIPI-DSI) | RST=5, Backlight=23 (LEDC PWM). DSI is 2 PHY lanes @ 500Mbps, not GPIOs. |
| Touch I2C (GT911)     | SDA=7, SCL=8 (shared with codec I2C), RST=3, INT unused (polled); address 0x5D, falls back to 0x14 |
| Audio I2S (ES8311)    | MCLK=13, BCK=12, WS=10, DOUT=9, DIN=48 |
| Codec I2C (ES8311)    | SDA=7, SCL=8 (same bus as touch) |
| Amp enable (NS4150)   | GPIO11 - polarity not documented, assumed active-high here, see audio-out.ino |
| SD (SDMMC, 4-bit)     | CLK=43, CMD=44, D0=39, D1=40, D2=41, D3=42. TF_VCC is powered from on-chip LDO channel 4 @ 3.3V, not a GPIO - see sdmmc-test.ino |
| Wi-Fi/BT (ESP32-C6 over ESP-Hosted SDIO) | CLK=18, CMD=19, D0=14, D1=15, D2=16, D3=17, C6 reset=54. Separate bus from the SD card's SDMMC, so both run together - see wifi-test.ino |

## Setup

1.  Install [guition-jc4880p4-bsp](https://github.com/ultramcu/guition-jc4880p4-bsp)
    as an Arduino library (not in Library Manager - clone it into your
    sketchbook's `libraries/` folder).
2.  Install its `esp_lcd_touch_gt911` dependency. It's not a registered
    Arduino library either - the BSP vendors its own copy per-example
    for PlatformIO at `examples/DisplayTouchTest/lib/esp_lcd_touch_gt911/`.
    For Arduino IDE/arduino-cli, copy that folder's `esp_lcd_touch.c`,
    `esp_lcd_touch_gt911.c`, and the two headers from its `include/`
    subfolder into a new library folder (e.g.
    `libraries/esp_lcd_touch_gt911/`), with the headers at the folder's
    top level, not nested under `include/` - Arduino's legacy library
    layout doesn't add an `include/` subfolder to the compiler's search
    path the way PlatformIO does.
3.  That header also needs one small patch to build under Arduino: it
    uses `CONFIG_ESP_LCD_TOUCH_MAX_POINTS`, a Kconfig macro that only
    exists in a real ESP-IDF/sdkconfig build. Add a fallback near the
    top of `esp_lcd_touch.h` (GT911 supports up to 5 points, matching
    ESP-IDF's own default):
    ```c
    #ifndef CONFIG_ESP_LCD_TOUCH_MAX_POINTS
    #define CONFIG_ESP_LCD_TOUCH_MAX_POINTS 5
    #endif
    ```
4.  Build with FQBN `esp32:esp32:esp32p4` (or your board manager's
    equivalent).

## Examples

-   `lcd-test` - draws 8 vertical color bars via
    guition-jc4880p4-bsp's framebuffer API, then polls GT911 touch and
    prints native (480x800 portrait) coordinates over Serial. All
    display/touch bring-up is delegated to that library; this file only
    calls its public `board_p4.h` API.
-   `audio-out` - sine wave playback through the ES8311/NS4150 path via
    `I2SCodecStream`. No existing `arduino-audio-driver` board entry
    covers this board, so the example uses the driver's `GenericES8311`
    board (the generic ES8311 codec driver with no predefined pins -
    same pattern the wiki documents for `GenericWM8960`/ES8388): `Wire`
    is set up manually for codec I2C, I2S pins are set directly on the
    stream config, and the NS4150 amp-enable pin is toggled manually
    (there's no `PinFunction::PA` pin on a `Generic*` board).
-   `audio-in` - reads the ES8311's ADC path and prints PCM samples to
    Serial as CSV, via `I2SCodecStream`/`GenericES8311` (see
    `audio-out` above). Whether an actual microphone is physically
    populated on this board (feeding that ADC input) isn't confirmed
    anywhere in the BSP or its docs - the speaker path is explicit,
    documented hardware; the mic path's presence is inferred only from
    the codec having one.
-   `sdmmc-test` - brings up the microSD card over `SD_MMC` in 4-bit
    mode, writes/reads a test file, lists the root directory. Needs
    `SD_MMC.setPowerChannel(4)` before `begin()` on this board - see its
    header comment and the Pins table above.
-   `player-sdmmc` - MP3 playback off the microSD card
    (`AudioSourceSDMMC` + `MP3DecoderHelix`) through the ES8311/NS4150
    path via `I2SCodecStream`/`GenericES8311`.
-   `wifi-test` - connects to a Wi-Fi AP via the onboard ESP32-C6
    (ESP-Hosted over SDIO) and prints the assigned IP. Needs
    `hostedSetPins()` before `WiFi.begin()` since this board's C6 SDIO
    wiring doesn't match arduino-esp32's default pins - see its header
    comment for a real, BSP-documented firmware-version-mismatch risk
    on this specific board's Wi-Fi.

## Dependencies

-   [arduino-audio-tools](https://github.com/pschatzmann/arduino-audio-tools)
    (`audio-out`, `audio-in`, `player-sdmmc`)
-   [arduino-audio-driver](https://github.com/pschatzmann/arduino-audio-driver)
    (`audio-out`, `audio-in`, `player-sdmmc`)
-   [arduino-libhelix](https://github.com/pschatzmann/arduino-libhelix)
    (`player-sdmmc`)
-   [guition-jc4880p4-bsp](https://github.com/ultramcu/guition-jc4880p4-bsp)
    (`lcd-test`)
-   `esp_lcd_touch_gt911` (`lcd-test`) - see Setup above
-   `wifi-test` needs no extra library - arduino-esp32's bundled `WiFi`
    library and its `esp32-hal-hosted.h` ESP-Hosted API are enough.
