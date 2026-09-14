//
// LED Example for STM32F411 Discovery Board
// This example toggles the on-board LEDs every second
//

#define LED_GREEN PD12
#define LED_BLUE PD15
#define LED_RED PD14
#define LED_ORANGE PD13

void led_toggle() {
  static bool active = true;
  Serial.print(active);
  digitalWrite(LED_GREEN, active);
  digitalWrite(LED_ORANGE, active);
  digitalWrite(LED_RED, active);
  digitalWrite(LED_BLUE, active);
  active = !active;
}

void setup() {
  Serial.begin(9600);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_ORANGE, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
}

void loop() {
  led_toggle();
  delay(1000);
}
