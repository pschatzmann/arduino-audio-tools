#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"
#include "AudioLibs/AudioRealFFT.h" // or AudioKissFFT
#include "AudioLibs/LEDMatrix.h"

#define PIN_LEDS 22
#define LED_X 32
#define LED_Y 8

AudioKitStream kit;  // Audio source
AudioRealFFT fft; // or AudioKissFFT
StreamCopy copier(fft, kit);  // copy mic to tfl
LEDMatrix led(fft); // output to LED matrix

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // setup Audiokit as input device
  auto cfg = kit.defaultConfig(RX_MODE);
  cfg.input_device = AUDIO_HAL_ADC_INPUT_LINE2;
  kit.begin(cfg);

  // Setup FFT output
  auto tcfg = fft.defaultConfig();
  tcfg.length = LED_X * 2; 
  tcfg.copyFrom(cfg);
  fft.begin(tcfg);

  // Setup LED matrix output
  auto lcfg = led.defaultConfig();
  lcfg.x = LED_X;
  lcfg.y = LED_Y;
  led.begin(lcfg);

  // addLeds
  FastLED.addLeds<WS2812B, PIN_LEDS, GRB>(led.ledData(), led.ledCount());
}

void loop() { 
  copier.copy();
  // update LEDs
  led.update();
}