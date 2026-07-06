## USB Audio

In addition to the examples in this directory, I recommend using the
**USB Audio** functionality available for STM32 boards.

See: `arduino-audio-tools/examples/examples-communication/usb`

Just compile these sketches with USB Support: NONE

## Display

Unlike the STM32F411 Discovery, this board does **not** include an
on-board IMU (gyroscope, accelerometer, or magnetometer). Instead, it
features a **1.54" 240×240 TFT display** based on the **ST7789H2**
controller, connected via the **FMC** peripheral.

See the `screen` example.

The display uses a **16-bit FMC interface**, which is not supported by
the common Arduino display libraries. Consequently, the example drives
the display directly through the STM32 HAL while exposing it as an
`Adafruit_GFX` subclass. This allows you to use the standard **Adafruit
GFX Library** (install it from the Arduino Library Manager) for fonts,
text rendering, and graphics primitives without reimplementing them.

## Audio

This board uses a **WM8994** audio codec instead of the **CS43L22**
found on the STM32F411 Discovery. The codec is connected via **SAI2**
rather than the traditional I2S peripheral.

The `audio-out` example serves as a starting point. However, full
functionality currently requires:

-   SAI2 support in the `stm32-i2s` backend:
    https://github.com/pschatzmann/stm32f411-i2s
-   Matching `PeripheralPins.c` definitions for this board in the
    Arduino STM32 core.

## Wi-Fi Module

The board includes a socket for an optional Wi-Fi module. Unlike some
other ST boards that use the SPI-based **Inventek ISM43362**, this board
is designed for a standard **ESP8266/ESP32 AT firmware** module.

The module is connected through:

-   **UART5** (PD2/PC12)
-   CH_PD
-   GPIO0
-   GPIO2
-   RST

See the `wifi` example.

The example uses the **WiFiEspAT** library (install it via the Arduino
Library Manager), providing the standard Arduino WiFi API. Since
`WiFiClient` is a genuine `Client` subclass, it works transparently with
libraries such as `HttpClient`, `PubSubClient` (MQTT), and any other
library that accepts a `Client&`.

### Updating the AT Firmware

If `WiFi.init()` fails even though manual `AT` commands work, your
module may be running an older AT firmware that does **not** support
`AT+CIPRECVMODE`.

WiFiEspAT depends on passive receive mode because `WiFiClient::read()`
and `available()` are implemented using `AT+CIPRECVDATA`, which only
functions after `AT+CIPRECVMODE` has been enabled.

To update the firmware, use the `wifi-esp-flash-bridge` example. It
turns the board into a transparent USB-to-UART bridge while
automatically placing the module into bootloader mode.

### Tested Configuration

The onboard module was verified on real hardware and identified as an
**ESP8266EX with 1 MB of flash**.

After reflashing with Espressif's Nano AT firmware (version
**1.7.6.0**), Wi-Fi scanning, connection, and `localIP()` all worked
correctly.

## External PSRAM

The board also includes a **512 KB IS61WV51216BLL-10MLI PSRAM** chip.

Like the LCD, it is connected to the FMC bus, but uses a different
chip-select:

-   **PSRAM:** NE1 / Bank 1 (`0x60000000`)
-   **LCD:** NE2 / Bank 2 (`0x64000000`)

Support for the PSRAM---including initialization, a heap allocator, and
a C++ STL allocator---is provided by the separate **stm32f723-psram**
library:

https://github.com/pschatzmann/stm32f723-psram
