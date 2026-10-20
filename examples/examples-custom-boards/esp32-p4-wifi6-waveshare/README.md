## Waveshare ESP32-P4-WIFI6

**Status: TESTED** on real hardware: `audio-out` (tone audible on the
speaker), `audio-in` (live mic samples), `sdmmc-test` (1-bit),
`player-sdmmc` (MP3 found and decoded) and `wifi-test` (scan finds access
points) run as expected. Wi-Fi association is not yet confirmed. The pins
come from Waveshare's own BSP (see below).

## Hardware

-   **MCU:** ESP32-P4 (dual-core RISC-V HP + single-core LP), 32MB
    in-package PSRAM, 32MB NOR flash
-   **Wi-Fi 6/BLE 5:** ESP32-C6-MINI-1 co-processor over SDIO
    (ESP-Hosted)
-   **Audio:** ES8311 codec + speaker amp (MX1.25 2P, 8Ω 2W speaker),
    onboard SMD microphone
-   **Storage:** TF card slot (SDMMC)
-   **Display/Camera:** MIPI-DSI 2-lane and MIPI-CSI 2-lane connectors
    (no panel on the board itself)
-   USB 2.0 HS OTG, 2 × 20 pin headers, BOOT and RESET buttons

## Pins

Source: Waveshare's [esp32_p4_platform BSP](https://github.com/waveshareteam/Waveshare-ESP32-components/tree/master/bsp/esp32_p4_platform)
(`include/bsp/esp32_p4_platform.h`), which Waveshare's
[ESP32-P4-Platform](https://github.com/waveshareteam/ESP32-P4-Platform)
`12_I2SCodec` and `09_sdmmc` examples use for the ESP32-P4-WIFI6. The
Wi-Fi pins are arduino-esp32's ESP32-P4 defaults, which Waveshare's own
Arduino Wi-Fi example relies on.

| Function           | Pins |
| ------------------ | ---- |
| Audio I2S (ES8311) | MCLK=13, BCK=12, WS=10, DOUT=9, DIN=11 |
| Codec I2C (ES8311) | SDA=7, SCL=8 |
| Amp enable         | GPIO53, active-high |
| SD (SDMMC)         | CLK=43, CMD=44, D0=39, D1=40, D2=41, D3=42, power from on-chip LDO channel 4. These are arduino-esp32's ESP32-P4 defaults, so the sketches don't set them. The core also drives GPIO45 LOW as an SD power pin; don't use GPIO45 for anything else alongside SD_MMC |
| Wi-Fi/BT (ESP32-C6 over ESP-Hosted SDIO) | CLK=18, CMD=19, D0=14, D1=15, D2=16, D3=17, C6 reset=54 |

## Setup

Build with FQBN `esp32:esp32:esp32p4:ChipVariant=postv3` (Arduino IDE:
Tools > Chip Variant > "v3.00 or newer"). This board ships with a v3.x
ESP32-P4 (ROM reports `esp32p4-eco7`); the default "Before v3.00" build
boot-loops with `rst:0x10 (CHIP_LP_WDT_RESET)` right after the bootloader.

## Examples

-   `audio-out` - sine wave playback through the ES8311 via
    `I2SCodecStream` and arduino-audio-driver's `WaveshareESP32P4WIFI6`
    board, which defines the codec I2C/I2S pins and enables the speaker
    amp - no pins need to be set in the sketch.
-   `audio-in` - reads the onboard microphone through the ES8311's ADC
    path (same `WaveshareESP32P4WIFI6` board) and prints PCM samples to
    Serial as CSV.
-   `sdmmc-test` - brings up the TF card over `SD_MMC` in 1-bit mode (4-bit
    mounts and reads, but writes fail with CRC errors on this board),
    writes/reads a test file, lists the root directory.
-   `player-sdmmc` - MP3 playback off the TF card (`AudioSourceSDMMC` +
    `MP3DecoderHelix`) through the ES8311.
-   `wifi-test` - scans for access points via the onboard ESP32-C6, then
    connects and prints the assigned IP. With arduino-esp32 3.3.10 the C6
    does not answer the firmware-version request (reported as `0.0.0`,
    host is 2.12.8); scanning works anyway. If connecting misbehaves,
    update the C6 firmware to the version the log suggests.

## Dependencies

-   [arduino-audio-tools](https://github.com/pschatzmann/arduino-audio-tools)
-   [arduino-audio-driver](https://github.com/pschatzmann/arduino-audio-driver)
-   [codec-helix](https://github.com/pschatzmann/codec-helix) (`player-sdmmc`)
-   `wifi-test` needs nothing beyond arduino-esp32's bundled `WiFi`.

## Further Info

-   https://docs.waveshare.com/ESP32-P4-WIFI6
