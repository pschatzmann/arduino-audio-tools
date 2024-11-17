/**
 * @file basic-a2dp-fft-led.ino
 * @brief A2DP Sink with output of the FFT result to the LED matrix   
 * For details see the FFT Wiki: https://github.com/pschatzmann/arduino-audio-tools/wiki/FFT
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include <FastLED.h> // to prevent conflicts introduced with 3.9
#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioRealFFT.h" // or any other supported inplementation
#include "AudioTools/AudioLibs/LEDOutput.h"
#include "BluetoothA2DPSink.h"

#define PIN_LEDS 22
#define LED_X 32
#define LED_Y 8

BluetoothA2DPSink a2dp_sink;
AudioRealFFT fft; // or any other supported inplementation
FFTDisplay fft_dis(fft);
LEDOutput led(fft_dis); // output to LED matrix

// Provide data to FFT
void writeDataStream(const uint8_t *data, uint32_t length) {
  fft.write(data, length);
}

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // Setup FFT
  auto tcfg = fft.defaultConfig();
  tcfg.length = 1024;
  tcfg.channels = 2;
  tcfg.sample_rate = a2dp_sink.sample_rate();;
  tcfg.bits_per_sample = 16;
  fft.begin(tcfg);

  // Setup LED matrix output
  auto lcfg = led.defaultConfig();
  lcfg.x = LED_X;
  lcfg.y = LED_Y;
  led.begin(lcfg);

  fft_dis.fft_group_bin = 3;
  fft_dis.fft_start_bin = 0;
  fft_dis.fft_max_magnitude = 40000;
  fft_dis.begin();

  // add LEDs
  FastLED.addLeds<WS2812B, PIN_LEDS, GRB>(led.ledData(), led.ledCount());

  // register A2DP callback
  a2dp_sink.set_stream_reader(writeDataStream, false);

  // Start Bluetooth Audio Receiver
  Serial.print("starting a2dp-fft...");
  a2dp_sink.set_auto_reconnect(false);
  a2dp_sink.start("a2dp-fft");

}

void loop() { 
  led.update();
  delay(50);
}