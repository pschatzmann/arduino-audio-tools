#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"
#include "AudioLibs/AudioRealFFT.h" // or AudioKissFFT
#include "AudioLibs/LEDOutput.h"

#define PIN_LEDS 22
#define LED_X 32
#define LED_Y 8

AudioKitStream kit;  // Audio source
AudioRealFFT fft; // or AudioKissFFT
StreamCopy copier(fft, kit);  // copy mic to fft
LEDOutput led(fft); // output to LED matrix

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // setup Audiokit as input device
  auto cfg = kit.defaultConfig(RX_MODE);
  cfg.input_device = AUDIO_HAL_ADC_INPUT_LINE2;
  kit.begin(cfg);

  // Setup FFT output
  auto tcfg = fft.defaultConfig();
  tcfg.length = 1024; 
  tcfg.copyFrom(cfg);
  fft.begin(tcfg);

  // Setup LED matrix output
  auto lcfg = led.defaultConfig();
  lcfg.x = LED_X;
  lcfg.y = LED_Y;
  lcfg.fft_group_bin = 3;
  lcfg.fft_start_bin = 0;
  lcfg.fft_max_magnitude = 40000;
  led.begin(lcfg);

  // add LEDs
  FastLED.addLeds<WS2812B, PIN_LEDS, GRB>(led.ledData(), led.ledCount());
}

void loop() { 
  // update FFT
  copier.copy();
  // update LEDs
  led.update();
}