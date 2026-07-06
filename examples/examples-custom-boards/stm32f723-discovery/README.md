Apart from the sketches that you can find in this directory, I recommend that you use the __USB audio functionality__ that is available for the STM32.

See arduino-audio-tools/examples/examples-communication/usb

Unlike the STM32F411 Discovery, this board has no on-board IMU (gyro/accelerometer/magnetometer).
Instead it has an on-board 1.54" 240x240 TFT display (ST7789H2 controller, connected via FMC) -
see the `screen` example. The panel's bus (16-bit FMC) isn't supported by any of the popular
Arduino display libraries, so the sketch drives it directly via HAL, but wraps that as an
`Adafruit_GFX` subclass (install "Adafruit GFX Library" from the Library Manager) so fonts,
text layout and shapes come from that library instead of being reimplemented.

The audio codec on this board is a WM8994 (not the CS43L22 used on the F411 Discovery), connected
via SAI2 instead of the classic I2S peripheral. The `audio-out` example is provided as a template,
but getting it fully working still requires SAI2 support in the low-level `stm32-i2s` backend
(https://github.com/pschatzmann/stm32f411-i2s) and matching `PeripheralPins.c` entries for this
board's Arduino core variant.

The board also has a socket labelled for a Wi-Fi module. It's wired as a plain ESP8266/ESP32
AT-firmware module (UART5 on PD2/PC12, plus CH_PD/GPIO0/GPIO2/RST control pins), not the SPI-based
Inventek ISM43362 used on some other ST boards - see the `wifi` example, which uses the
"WiFiEspAT" library (install via the Library Manager) to talk to it with the standard Arduino
WiFi API. `WiFiClient` there is a real `Client` subclass, so it's drop-in compatible with anything
that takes a `Client&` (`HttpClient`, `PubSubClient`/MQTT, etc.) - the example demonstrates it with
a plain HTTP GET.

If `WiFi.init()` fails even though raw `AT` commands work, the module's AT firmware may predate
`AT+CIPRECVMODE`, which WiFiEspAT's TCP/UDP read path depends on unconditionally (not just an
optional extra - `WiFiClient::read()`/`available()` are built entirely on `AT+CIPRECVDATA`, which
only works once passive mode is on). If it does need updating, see the `wifi-esp-flash-bridge`
example: it turns the board into a transparent USB<->module UART bridge (putting the module into
UART bootloader mode itself, since esptool's usual DTR/RTS auto-reset trick can't reach this
module's RST/GPIO0 pins through a plain serial port) so `esptool.py` on your PC can identify the
chip and reflash it directly.

Confirmed on real hardware: the on-board module here was an **ESP8266EX with 1MB flash**, whose
factory AT firmware didn't support `CIPRECVMODE`. Fixed by reflashing Espressif's "Nano AT" build
(`ESP8266_NONOS_SDK`'s `bin/at/512+512`, the variant for 1MB flash - the normal 1024+1024 build
needs 2MB+) via `esptool.py write_flash` at `0x00000`/`0x01000`/`0xfc000`/`0x7e000`/`0xfe000` with
`boot_v1.7.bin`/`user1.1024.new.2.bin`/`esp_init_data_default_v08.bin`/`blank.bin`
(`--flash_mode dout --flash_size detect`) - all from that SDK's `bin/` and `bin/at/512+512/`
folders. Confirms Nano AT does include `CIPRECVMODE` despite the SDK's README only documenting a
reduced SSL cipher-suite set for it, not a reduced command set. After reflashing (chip reports
firmware `1.7.6.0`), scan/connect/`localIP()` all work correctly.

There's also a 512KB external PSRAM chip (IS61WV51216BLL-10MLI), on the same FMC bus as the LCD
but on a different chip-select (NE1/Bank1 at `0x60000000`, vs. the LCD's NE2/Bank2 at
`0x64000000`). Bring-up, a heap allocator, and a C++ STL `Allocator` for it now live in their own
library - [stm32f723-psram](https://github.com/pschatzmann/arduino-audio-tools) (install it, then
see its own README and `PsramTest`/`PsramSTL` examples) - rather than duplicated here, since none
of it is audio-specific.
