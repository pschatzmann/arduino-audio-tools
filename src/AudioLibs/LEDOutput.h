#pragma once
#include <FastLED.h>
#include "AudioLibs/AudioFFT.h"

namespace audio_tools {
class LEDOutput;
struct LEDOutputConfig;

LEDOutput *selfLEDOutput=nullptr;
// default callback function which implements led update
void updateLEDOutput(LEDOutputConfig*cfg, LEDOutput *matrix, int  max_y);
// default color
CHSV getColor(int x, int y, int magnitude){
  return CHSV( 224, 187, 100);
}

/**
 * LED Matrix Configuration. Provide the number of leds in x and y direction and
 * the data pin.
 * @author Phil Schatzmann
 */
struct LEDOutputConfig {
  /// Number of leds in x direction
  int x = 0;
  /// Number of leds in y direction
  int y = 0;
  /// optinal custom logic to provide CHSV color: Prividing a 'rainbow' color with hue 0-255, saturating 0-255, and brightness (value) 0-255 (v2)
  CHSV (*color_callback)(int x, int y, int magnitude) = getColor;
  /// Custom callback logic to update the LEDs - by default we use updateLEDOutput()
  void (*update_callback)(LEDOutputConfig*cfg, LEDOutput *matrix, int  max_y) = updateLEDOutput;
  /// Update the leds only ever nth call
  int update_frequency = 1; // update every call
};

/**
 * LEDOutput using the FastLED library. You write the data to the FFT Stream.
 * This displays the result of the FFT to a LED matrix.
 * @ingroup io
 * @author Phil Schatzmann
 */
class LEDOutput {
public:
  /// @brief Default Constructor
  /// @param fft 
  LEDOutput(AudioFFTBase &fft) {
    selfLEDOutput = this;
    p_fft = &fft;
  }

  /// Provides the default config object
  LEDOutputConfig defaultConfig() { return cfg; }

  /// Setup Led matrix
  bool begin(LEDOutputConfig config) {
    cfg = config;
    if (!*p_fft) {
      LOGE("fft not started");
      return false;
    }
    if (ledCount() == 0) {
      LOGE("x or y == 0");
      return false;
    }
    // assign fft callback
    AudioFFTConfig &fft_cfg = p_fft->config();
    fft_cfg.callback = fftCallback;

    // allocate leds
    leds.resize(ledCount());

    // if the number of bins > number of leds in x position we combine adjecent
    // values
    magnitude_div = p_fft->size() / cfg.x;

    // number of bins
    magnitudes.resize(p_fft->size());

    return true;
  }

  /// Provides the number of LEDs: call begin() first!
  int ledCount(){
    int num_leds = cfg.x * cfg.y;
    return num_leds;
  }

  ///  Provides the address fo the CRGB array: call begin() first!
  CRGB* ledData() {
    if (ledCount() == 0) {
      LOGE("x or y == 0");
      return nullptr;
    }
    leds.resize(ledCount());
    return leds.data();
  }

  /// Updates the display: call this method in your loop
  virtual void update() {
    if (cfg.update_callback!=nullptr && count++ % cfg.update_frequency == 0) {
      // use custom update logic defined in config
      cfg.update_callback(&cfg, this, max_y);
    }
  }

  /// Determine the led with the help of the x and y pos
  CRGB &xyLed(uint8_t x, uint8_t y) {
    int index = (y * cfg.x) + x;
    return leds[index];
  }

  /// Returns the magnitude for the indicated led x position. We might
  /// need to combine values from the magnitudes array if this is much bigger.
  float getMagnitude(int x) {
    float total = 0;
    int mag_x = x * magnitude_div;
    for (int j = 0; j < magnitude_div;j++) {
      int x = mag_x + j;
      if (x >= p_fft->size()) {
        break;
      }
      total += magnitudes[x];
    }
    // determine max y to scale output
    if (selfLEDOutput->max_y > total) {
      selfLEDOutput->max_y = total;
    }
    return total;
  }

  /// Provides access to the actual config object. E.g. to change the update logic 
  LEDOutputConfig &config() {
    return cfg;
  }

protected:
  friend class AudioFFTBase;
  Vector<CRGB> leds{0};
  Vector<float> magnitudes{0};
  LEDOutputConfig cfg;
  int magnitude_div = 1;
  AudioFFTBase *p_fft = nullptr;
  float max_y = 1000.0f;
  uint64_t count = 0;


  /// callback method which provides updated data from fft
  static void fftCallback(AudioFFTBase &fft) {
    // just save magnitudes to be displayed
    for (int j = 0; j < fft.size(); j++) {
      float value = fft.magnitude(j);
      selfLEDOutput->magnitudes[j] = value;
    }
  };
};

/// Default update implementation which provides the fft result as "barchart"
void updateLEDOutput(LEDOutputConfig*cfg, LEDOutput *matrix, int  max_y){
  for (int x = 0; x < cfg->x; x++) {
    // max y determined by magnitude
    int maxY = mapFloat(matrix->getMagnitude(x), 0.0f, max_y, 0.0f,
                        static_cast<float>(cfg->y));
    if (maxY>cfg->y){
      maxY = cfg->y;
    }
    // update horizontal bar
    for (int y = 0; y < maxY; y++) {
      // determine color
      CHSV color = cfg->color_callback(x, y, maxY);
      // update LED
      matrix->xyLed(x, y) = color;
    }
  }
  FastLED.show();
}


} // namespace audio_tools