/**
 * @file wifi-test.ino
 * @brief Wi-Fi connect test for the Guition JC4880P443C_I_W (ESP32-P4):
 * connects to an access point and prints the assigned IP address. On
 * this board, Wi-Fi isn't a P4 peripheral at all - the P4 has no radio.
 * It's provided by the onboard ESP32-C6, reached over SDIO using
 * arduino-esp32's "ESP-Hosted" remote-Wi-Fi driver, and used through
 * the ordinary `WiFi.h` API exactly as on a single-chip board.
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
