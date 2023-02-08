#pragma once
#include <FastLED.h>
#include "AudioLibs/AudioFFT.h"

namespace audio_tools {

void *selfLEDMatrix=nullptr;

/**
 * LED Matrix Configuration. Provide the number of leds in x and y direction and
 * the data pin.
 */
struct LEDMatrixConfig {
  int x = 0;
  int y = 0;
  int color = CRGB::Blue;
  int (*get_color)(int x, int y, int magnitude) = nullptr;
  int update_frequency = 1; // update every call
};

/**
 * LEDMatrix using the FastLED library. You write the data to the FFT Stream.
 * This displays the result of the FFT to a LED matrix.
 */
class LEDMatrix {
public:
  LEDMatrixConfig defaultConfig() { return cfg; }

  LEDMatrix(AudioFFTBase &fft) {
    selfLEDMatrix = this;
    p_fft = &fft;
    AudioFFTConfig &fft_cfg = p_fft->config();
    fft_cfg.callback = fftCallback;
  }

  /// Setup Led matrix
  bool begin(LEDMatrixConfig config) {
    cfg = config;
    if (!*p_fft) {
      LOGE("fft not started");
      return false;
    }
    if (ledCount() == 0) {
      LOGE("x or y == 0");
      return false;
    }

    // if the number of bins > number of leds in x position we combine adjecent
    // values
    magnitude_div = p_fft->size() / cfg.x;

    // number of bins
    magnitudes.resize(p_fft->size());
  }

  // Provides the number of LEDs: call begin() first!
  int ledCount(){
    int num_leds = cfg.x * cfg.y;
    return num_leds;
  }

  /// @brief  Provides the address fo the CRGB array: call begin() first!
  /// @return 
  CRGB* ledData() {
    if (ledCount() == 0) {
      LOGE("x or y == 0");
      return nullptr;
    }
    leds.resize(ledCount());
    return leds.data();
  }

  /// Updates the display: call this method in your loop
  void update() {
    if (count++ % cfg.update_frequency == 0) {
      for (int x = 0; x < cfg.x; x++) {
        // max y determined by magnitude
        int maxY = mapFloat(getMagnitude(x), 0.0f, max_y, 0.0f,
                            static_cast<float>(cfg.y));
        // update horizontal bar
        for (int y = 0; y < maxY; y++) {
          int color = cfg.color;
          if (cfg.get_color != nullptr) {
            color = cfg.get_color(x, y, maxY);
          }
          xyLed(x, y) = color;
        }
      }
      FastLED.show();
    }
  }

protected:
  friend class AudioFFTBase;
  Vector<CRGB> leds{0};
  Vector<float> magnitudes{0};
  LEDMatrixConfig cfg;
  int magnitude_div = 1;
  AudioFFTBase *p_fft = nullptr;
  float max_y = 1000.0f;
  uint64_t count = 0;

  // determine index of the led with the help of the x and y pos
  CRGB &xyLed(uint8_t x, uint8_t y) {
    int index = (y * cfg.x) + x;
    return leds[index];
  }

  /// @brief  returns the magnitude for the indicated led x position. We might
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
    LEDMatrix* self = static_cast<LEDMatrix*>(selfLEDMatrix);
    if (self->max_y > total) {
      self->max_y = total;
    }
    return total;
  }

  /// callback method which provides updated data from fft
  static void fftCallback(AudioFFTBase &fft) {
    // just save magnitudes to be displayed
    LEDMatrix* self = static_cast<LEDMatrix*>(selfLEDMatrix);

    for (int j = 0; j < fft.size(); j++) {
      float value = fft.magnitude(j);
      self->magnitudes[j] = value;
    }
  };
};

} // namespace audio_tools