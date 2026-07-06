//
// LED Example for STM32F723E Discovery Board
// This example toggles the on-board LEDs every second
// Unlike the F411 Discovery, this board only has 2 user LEDs
//

#define LED_RED PA7
#define LED_GREEN PB1

void led_toggle() {
  static bool active = true;
  Serial.print(active);
  digitalWrite(LED_RED, active);
  digitalWrite(LED_GREEN, active);
  active = !active;
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
}

void loop() {
  led_toggle();
  delay(1000);
}
