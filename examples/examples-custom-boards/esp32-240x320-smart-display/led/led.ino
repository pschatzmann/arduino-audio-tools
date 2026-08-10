/**
 * @file led.ino
 * @brief RGB status LED cycle example for the ESP32 Arduino LVGL
 * WiFi/Bluetooth Development Board with 2.4" LCD TFT module
 * @see
 * https://github.com/pschatzmann/arduino-audio-tools/wiki/Audio-Boards#esp32-arduino-lvgl-wifibluetooth-development-board-24inch-lcd-tft-module
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#define LED_R 4
#define LED_G 16
#define LED_B 17

void setup() {
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);

  // Start with LED OFF
  digitalWrite(LED_R, HIGH);
  digitalWrite(LED_G, HIGH);
  digitalWrite(LED_B, HIGH);
}

void loop() {
  // RED
  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, HIGH);
  digitalWrite(LED_B, HIGH);
  delay(1000);

  // GREEN
  digitalWrite(LED_R, HIGH);
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_B, HIGH);
  delay(1000);

  // BLUE
  digitalWrite(LED_R, HIGH);
  digitalWrite(LED_G, HIGH);
  digitalWrite(LED_B, LOW);
  delay(1000);

  // YELLOW = RED + GREEN
  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_B, HIGH);
  delay(1000);

  // CYAN = GREEN + BLUE
  digitalWrite(LED_R, HIGH);
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_B, LOW);
  delay(1000);

  // MAGENTA = RED + BLUE
  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, HIGH);
  digitalWrite(LED_B, LOW);
  delay(1000);

  // WHITE = RED + GREEN + BLUE
  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_B, LOW);
  delay(1000);

  // OFF
  digitalWrite(LED_R, HIGH);
  digitalWrite(LED_G, HIGH);
  digitalWrite(LED_B, HIGH);
  delay(1000);
}
