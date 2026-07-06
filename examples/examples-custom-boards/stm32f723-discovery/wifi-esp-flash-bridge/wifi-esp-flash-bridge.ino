//
// ESP Wi-Fi Module UART Flash Bridge for STM32F723E Discovery Board
//
// Bridges the PC's serial connection (Serial, over the ST-Link VCP - a real
// UART on USART6, not native USB CDC on this MCU) straight through to the
// on-board Wi-Fi module's UART (UART5 on PD2/PC12), so esptool.py running
// on the PC can talk directly to the module's ROM bootloader to reflash
// its AT firmware. Useful once the currently-flashed AT firmware turns out
// to be missing something a library needs - see the `wifi` example's notes
// on AT+CIPRECVMODE, which is what led to needing this.
//
// esptool's usual trick of auto-resetting a module into bootloader mode by
// toggling a USB-serial adapter's DTR/RTS lines doesn't apply here: this
// module's RST/GPIO0 pins are separate STM32 GPIOs (PG14/PG13), not wired
// to anything the PC's serial connection can reach. So this sketch puts
// the module into UART download/bootloader mode itself (GPIO0 held LOW
// through a reset pulse) before handing the UART off as a dumb pipe.
//
// Usage:
//  1. Flash this sketch. Find which serial port is the STM32's ST-Link VCP
//     (Device Manager on Windows, `ls /dev/tty.*`/`ls /dev/ttyACM*` on
//     macOS/Linux) - do NOT open a serial monitor on it, esptool needs the
//     port free.
//  2. Identify the module first - don't guess the chip/flash size:
//       esptool.py --port <PORT> --baud 115200 --before no_reset \
//         --after no_reset flash_id
//     This prints the chip type (ESP8266EX / ESP32-xxx) and flash size.
//  3. Get an AT firmware build matching that chip (ESP8266: Espressif's
//     "ESP8266 NonOS AT Bin" releases; ESP32/ESP32-Cx: the "esp-at" project
//     binaries at https://github.com/espressif/esp-at/releases) and flash
//     it per that package's own instructions, e.g.:
//       esptool.py --port <PORT> --baud 115200 --before no_reset \
//         --after no_reset write_flash @download.config
//     (the exact files/offsets are chip- and package-specific - use the
//     download.config/README that ships with the firmware you downloaded,
//     don't reuse one from a different chip/package).
//  4. Re-flash the `wifi` example afterwards - its own reset sequence
//     drives GPIO0 HIGH again, booting the module normally into the new
//     firmware instead of the bootloader.
//

#define WIFI_RX PD2   // UART5 RX <- module TX
#define WIFI_TX PC12  // UART5 TX -> module RX
#define WIFI_CH_PD PD3
#define WIFI_GPIO0 PG13
#define WIFI_GPIO2 PD6
#define WIFI_RST PG14

#define BRIDGE_BAUD 115200

HardwareSerial WifiSerial(WIFI_RX, WIFI_TX);

void setup() {
  Serial.begin(BRIDGE_BAUD);

  pinMode(WIFI_CH_PD, OUTPUT);
  pinMode(WIFI_GPIO0, OUTPUT);
  pinMode(WIFI_GPIO2, OUTPUT);
  pinMode(WIFI_RST, OUTPUT);

  digitalWrite(WIFI_CH_PD, HIGH);  // module powered/enabled
  digitalWrite(WIFI_GPIO2, HIGH);  // must be HIGH for any valid boot mode
  digitalWrite(WIFI_GPIO0, LOW);   // LOW = UART download/bootloader mode

  // Reset with GPIO0 held low so the ROM bootloader comes up instead of
  // whatever AT firmware is currently flashed.
  digitalWrite(WIFI_RST, LOW);
  delay(50);
  digitalWrite(WIFI_RST, HIGH);
  delay(100);  // let the ROM bootloader finish starting up

  WifiSerial.begin(BRIDGE_BAUD);

  Serial.println("Bridge ready - module held in UART download mode.");
  Serial.println("Run esptool.py against this port now (see file header).");
}

void loop() {
  // Plain byte-for-byte relay in both directions, no buffering/parsing -
  // esptool needs a transparent pipe, not a smart terminal.
  while (Serial.available()) WifiSerial.write(Serial.read());
  while (WifiSerial.available()) Serial.write(WifiSerial.read());
}
