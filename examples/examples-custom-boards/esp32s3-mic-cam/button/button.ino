
/// If button is pressed it is pulled low

const int BUTTON = 38;

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON, INPUT);
}

void loop() {
  Serial.println(digitalRead(BUTTON));
}