// Demo for the color LED (SK6812)

#include <FastLED.h>

const int NUM_LEDS = 1;
const int DATA_PIN = 33;
CRGB led;

void setup() {
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(&led, NUM_LEDS);
}
void loop() {
  led = CRGB::Red;
  FastLED.show();
  delay(1000);
  led = CRGB::Black;
  FastLED.show();
  delay(1000);
}