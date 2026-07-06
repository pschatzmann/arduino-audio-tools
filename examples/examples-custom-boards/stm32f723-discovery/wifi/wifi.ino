//
// WiFi Example for STM32F723E Discovery Board
// The board has a socket labelled for a Wi-Fi module, wired as a plain
// ESP8266/ESP32 AT-firmware module (not SPI like the Inventek ISM43362 used
// on some other ST boards):
//  - UART5: PD2 (RX, <- module TX), PC12 (TX, -> module RX)
//  - WIFI_CH_PD: PD3  (module enable)
//  - WIFI_GPIO0: PG13 (boot mode select, HIGH = normal run)
//  - WIFI_GPIO2: PD6  (boot mode select, HIGH = normal run)
//  - WIFI_RST:   PG14 (module reset, handled by WiFiEspAT below)
//
// This uses the "WiFiEspAT" library (install via Library Manager), which
// talks to ESP8266/ESP32 AT firmware over a plain UART and exposes the
// standard Arduino WiFi API (WiFi.begin(), WiFi.localIP(), WiFiClient, ...).
// Fill in WIFI_SSID/WIFI_PASS below before flashing.
//
// If "WiFi module not found" shows up (AT firmware not responding at all)
// or WiFi.init() fails even though raw AT commands work (firmware missing
// AT+CIPRECVMODE, which WiFiEspAT's read path needs unconditionally), see
// this board's README - it covers both, including how to reflash the
// module's AT firmware via the `wifi-esp-flash-bridge` example.
//
// WiFiClient here is a real Client subclass (see WiFiClient.h), so it's
// drop-in compatible with anything that takes a Client& (HttpClient,
// PubSubClient/MQTT, etc.) - demonstrated below with a plain HTTP GET.
//

#include <WiFiEspAT.h>

#define WIFI_RX PD2   // UART5 RX <- module TX
#define WIFI_TX PC12  // UART5 TX -> module RX
#define WIFI_CH_PD PD3
#define WIFI_GPIO0 PG13
#define WIFI_GPIO2 PD6
#define WIFI_RST PG14
#define AT_BAUD_RATE 115200

HardwareSerial WifiSerial(WIFI_RX, WIFI_TX);

const char* WIFI_SSID = "your-ssid";      // <-- fill in
const char* WIFI_PASS = "your-password";  // <-- fill in

static void resetModule() {
  pinMode(WIFI_RST, OUTPUT);
  digitalWrite(WIFI_RST, LOW);
  delay(50);
  pinMode(WIFI_RST, INPUT);  // release - relies on the module's own pull-up
  delay(1000);               // let boot-time messages/noise settle
}

static const char *wlStatusName(uint8_t status) {
  switch (status) {
    case WL_IDLE_STATUS: return "WL_IDLE_STATUS";
    case WL_NO_SSID_AVAIL: return "WL_NO_SSID_AVAIL (SSID not seen in scan)";
    case WL_CONNECTED: return "WL_CONNECTED";
    case WL_CONNECT_FAILED: return "WL_CONNECT_FAILED (likely wrong password)";
    case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST";
    case WL_DISCONNECTED: return "WL_DISCONNECTED";
    case WL_NO_MODULE: return "WL_NO_MODULE";
    default: return "(unknown)";
  }
}

static const char *driverErrorName(EspAtDrvError err) {
  switch (err) {
    case EspAtDrvError::NO_ERROR: return "NO_ERROR";
    case EspAtDrvError::NOT_INITIALIZED: return "NOT_INITIALIZED";
    case EspAtDrvError::AT_NOT_RESPONDIG: return "AT_NOT_RESPONDING";
    case EspAtDrvError::AT_ERROR: return "AT_ERROR";
    case EspAtDrvError::NO_AP: return "NO_AP (join rejected - check password)";
    case EspAtDrvError::LINK_ALREADY_CONNECTED: return "LINK_ALREADY_CONNECTED";
    case EspAtDrvError::LINK_NOT_ACTIVE: return "LINK_NOT_ACTIVE";
    case EspAtDrvError::RECEIVE: return "RECEIVE";
    case EspAtDrvError::SEND: return "SEND";
    case EspAtDrvError::UDP_BUSY: return "UDP_BUSY";
    case EspAtDrvError::UDP_LARGE: return "UDP_LARGE";
    case EspAtDrvError::UDP_TIMEOUT: return "UDP_TIMEOUT";
    default: return "(unknown)";
  }
}

static void httpGetDemo() {
  const char *host = "example.com";
  Serial.print("Fetching http://");
  Serial.print(host);
  Serial.println("/ ...");

  WiFiClient client;
  if (!client.connect(host, 80)) {
    Serial.println("connection failed");
    return;
  }

  client.print("GET / HTTP/1.1\r\n");
  client.print("Host: ");
  client.print(host);
  client.print("\r\n");
  client.print("Connection: close\r\n\r\n");

  uint32_t start = millis();
  while (client.connected() && millis() - start < 10000) {
    while (client.available()) {
      Serial.write(client.read());
      start = millis();
    }
  }
  client.stop();
  Serial.println("\n--- connection closed ---");
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  // Boot-mode pins: both must be HIGH for normal run mode (LOW selects
  // UART firmware-flashing mode on ESP8266/ESP32).
  pinMode(WIFI_CH_PD, OUTPUT);
  pinMode(WIFI_GPIO0, OUTPUT);
  pinMode(WIFI_GPIO2, OUTPUT);
  digitalWrite(WIFI_CH_PD, HIGH);
  digitalWrite(WIFI_GPIO0, HIGH);
  digitalWrite(WIFI_GPIO2, HIGH);

  resetModule();
  WifiSerial.begin(AT_BAUD_RATE);

  // WiFi.init() also resets via WIFI_RST itself (briefly drives it low,
  // then releases to input - relies on the module's own pull-up on RST,
  // standard on ESP8266/ESP32 AT modules); the explicit reset above just
  // guarantees a clean slate before that happens too.
  Serial.println("Initializing WiFi module...");
  if (!WiFi.init(WifiSerial, WIFI_RST)) {
    Serial.println("WiFi module not found - check wiring and AT firmware");
    while (true) delay(1000);
  }

  Serial.print("AT firmware version: ");
  Serial.println(WiFi.firmwareVersion());

  Serial.println("Scanning networks...");
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    Serial.print(i);
    Serial.print(": ");
    Serial.print(WiFi.SSID(i));
    Serial.print("  (");
    Serial.print(WiFi.RSSI(i));
    Serial.println(" dBm)");
  }

  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t start = millis();
  const uint32_t timeoutMs = 20000;
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Failed to connect. WiFi.status() = ");
    Serial.print(wlStatusName(WiFi.status()));
    Serial.print(", last driver error = ");
    Serial.println(driverErrorName(WiFi.getLastDriverError()));
    Serial.println("Common causes: wrong password, or an AP that's WPA3-only "
                    "/ 5GHz-only (this ESP8266 only supports 2.4GHz WPA/WPA2).");
    while (true) delay(1000);
  }

  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());

  httpGetDemo();
}

void loop() {
  delay(1000);
}
