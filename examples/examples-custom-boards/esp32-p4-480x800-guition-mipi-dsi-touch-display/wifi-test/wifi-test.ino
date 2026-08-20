/**
 * @file wifi-test.ino
 * @brief Wi-Fi connect test for the Guition JC4880P443C_I_W (ESP32-P4):
 * connects to an access point and prints the assigned IP address. On
 * this board, Wi-Fi isn't a P4 peripheral at all - the P4 has no radio.
 * It's provided by the onboard ESP32-C6, reached over SDIO using
 * arduino-esp32's "ESP-Hosted" remote-Wi-Fi driver, and used through
 * the ordinary `WiFi.h` API exactly as on a single-chip board.
 *
 * UNTESTED - written without access to this hardware. Two real risks
 * specific to this board, both from guition-jc4880p4-bsp's README:
 *
 * 1. SDIO pins: ESP-Hosted's default pin set (baked into
 *    arduino-esp32's precompiled libs) doesn't match this board's
 *    wiring, so `hostedSetPins()` below must run before `WiFi.begin()`
 *    to point it at this board's actual CLK/CMD/D0-D3/C6-reset pins
 *    (from board_p4_pins.h's "Wi-Fi/BT" section).
 * 2. Firmware version mismatch: this board reportedly ships with C6
 *    ESP-Hosted slave firmware 2.3.0, while arduino-esp32 3.3.x's host
 *    side expects ~2.12 - a mismatch the BSP's README says causes a
 *    `Version mismatch` error and an association/drop/reset loop (Wi-Fi
 *    associates, then immediately disconnects, repeatedly). The
 *    documented fix is reflashing the C6 with `network_adapter_esp32c6.bin`
 *    2.12.9 directly over UART (board's JP1 header) - see the BSP's
 *    README "Wi-Fi via C6" section for the exact steps. If this example
 *    connects but then drops in a loop, that firmware mismatch is the
 *    first thing to check, not this sketch.
 *
 * Confirmed (by compiling, not on real hardware): the generic
 * `esp32:esp32:esp32p4` FQBN's precompiled libraries do have
 * ESP-Hosted support built in - `hostedSetPins()` and `WiFi.h` compile
 * against it without needing a custom board build.
 *
 * Dependencies: none beyond arduino-esp32's bundled WiFi library.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include <WiFi.h>
#include <esp32-hal-hosted.h>

// Wi-Fi (ESP32-C6 over ESP-Hosted SDIO), from
// guition-jc4880p4-bsp's board_p4_pins.h "Wi-Fi/BT" section.
constexpr int kPinWifiSdioClk = 18;
constexpr int kPinWifiSdioCmd = 19;
constexpr int kPinWifiSdioD0 = 14;
constexpr int kPinWifiSdioD1 = 15;
constexpr int kPinWifiSdioD2 = 16;
constexpr int kPinWifiSdioD3 = 17;
constexpr int kPinWifiC6Reset = 54;

const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";

void setup() {
  Serial.begin(115200);
  delay(2000);

  if (!hostedSetPins(kPinWifiSdioClk, kPinWifiSdioCmd, kPinWifiSdioD0,
                      kPinWifiSdioD1, kPinWifiSdioD2, kPinWifiSdioD3,
                      kPinWifiC6Reset)) {
    Serial.println("hostedSetPins() failed");
    while (true) delay(1000);
  }

  Serial.printf("Connecting to %s...\n", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  delay(5000);
  Serial.printf("Wi-Fi status: %d, RSSI: %d dBm\n", WiFi.status(),
                WiFi.RSSI());
}
