/**
 * @file streams-audiokit-fft-led.ino
 * @author Phil Schatzmann
 * @brief  We peform FFT on the microphone input of the AudioKit and display the
 * result on a 32*8 LED matrix
 * @version 0.1
 * @date 2022-10-14
 *
 * @copyright Copyright (c) 2022
 *
 */
#include <FastLED.h> // to prevent conflicts introduced with 3.9
#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/AudioLibs/AudioRealFFT.h"  // or AudioKissFFT
#include "AudioTools/AudioLibs/LEDOutput.h"

#define PIN_LEDS 22
#define LED_X 32
#define LED_Y 8

AudioBoardStream kit(AudioKitEs8388V1);  // Audio source
AudioRealFFT fft;    // or AudioKissFFT
FFTDisplay fft_dis(fft);
LEDOutput led(fft_dis);       // output to LED matrix
StreamCopy copier(fft, kit);  // copy mic to fft

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // setup Audiokit as input device
  auto cfg = kit.defaultConfig(RX_MODE);
  cfg.input_device = ADC_INPUT_LINE2;
  kit.begin(cfg);

  // Setup FFT output
  auto tcfg = fft.defaultConfig();
  tcfg.length = 1024;
  tcfg.copyFrom(cfg);
  fft.begin(tcfg);

  // Setup FFT Display
  fft_dis.fft_group_bin = 3;
  fft_dis.fft_start_bin = 0;
  fft_dis.fft_max_magnitude = 40000;
  fft_dis.begin();

  // Setup LED matrix output
  auto lcfg = led.defaultConfig();
  lcfg.x = LED_X;
  lcfg.y = LED_Y;
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