/**
 * @file streams-generator-audiokit.ino
 * @brief NeoPixel example for ESP32-S3-AI-Smart-Speaker
 * 
 * Dependencies:
 * - https://github.com/adafruit/Adafruit_NeoPixel
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include <Adafruit_NeoPixel.h>

// Which pin on the Arduino is connected to the NeoPixels?
#define PIN 38

// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS 6

// When setting up the NeoPixel library, we tell it how many pixels,
// and which pin to use to send signals. Note that for older NeoPixel
// strips you might need to change the third parameter -- see the
// strandtest example for more information on possible values.
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  // END of Trinket-specific code.
  pixels.begin();  // INITIALIZE NeoPixel strip object (REQUIRED)
}

void loop() {
  pixels.clear();  // Set all pixel colors to 'off'

  // The first NeoPixel in a strand is #0, second is 1, all the way up
  // to the count of pixels minus one.
  for (int i = 0; i < NUMPIXELS; i++) {  // For each pixel...

    // pixels.Color() takes RGB values, from 0,0,0 up to 255,255,255
    int red = random(255);
    int green = random(255);
    int blue = random(255);
    // Here we're using a moderately bright green color:
    pixels.setPixelColor(i, pixels.Color(red, green, blue));
  }
  pixels.show();  // Send the updated pixel colors to the hardware.
  delay(100);     // Pause before next pass through loop
}
