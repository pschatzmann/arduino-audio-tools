//
// Button Example for STM32F723E Discovery Board
// This example reads the state of the user button and prints it to the serial monitor
// The user button is connected to pin PA0 (WAKEUP): pressed gives 1, released gives 0
// Compile with
// - USB Support: CDC generic Serial suppercedes USART
// - USART Support: Enabled (Generic Serial)
//

#define USER_BUTTON PA0

void setup() {
  Serial.begin(115200);
  pinMode(USER_BUTTON, INPUT);
}

void loop() {
  bool active = digitalRead(USER_BUTTON);
  Serial.println(active);
  delay(1000);
}
