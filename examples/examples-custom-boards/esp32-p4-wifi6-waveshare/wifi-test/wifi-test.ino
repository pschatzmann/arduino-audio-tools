/**
 * @file wifi-test.ino
 * @brief Wi-Fi test for the Waveshare ESP32-P4-WIFI6: scans for access
 * points, then connects and prints the assigned IP address. The P4 has no
 * radio - Wi-Fi is provided by the onboard ESP32-C6, reached over SDIO
 * using arduino-esp32's "ESP-Hosted" driver, and used through the
 * ordinary `WiFi.h` API exactly as on a single-chip board.
 *
 * The C6 SDIO wiring matches arduino-esp32's ESP32-P4 defaults
 * (BOARD_SDIO_ESP_HOSTED_* in pins_arduino.h); they are passed explicitly
 * here only to document them.
 *
 * Dependencies: none beyond arduino-esp32's bundled WiFi library.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include <WiFi.h>
#include <esp32-hal-hosted.h>

// Wi-Fi (ESP32-C6 over ESP-Hosted SDIO)
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

  WiFi.mode(WIFI_STA);
  Serial.println("scanning...");
  int n = WiFi.scanNetworks();
  Serial.printf("%d networks found\n", n);
  for (int i = 0; i < n; i++) {
    Serial.printf("  %-32s %4d dBm  ch %2d\n", WiFi.SSID(i).c_str(),
                  WiFi.RSSI(i), WiFi.channel(i));
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
