#pragma once
#include "AudioLibs/AudioFFT.h"
#include <FastLED.h>

namespace audio_tools {
class LEDOutput;
struct LEDOutputConfig;

LEDOutput *selfLEDOutput = nullptr;
// default callback function which implements led update
void fftLEDOutput(LEDOutputConfig *cfg, LEDOutput *matrix);
// led update for volume
void volumeLEDOutput(LEDOutputConfig *cfg, LEDOutput *matrix);
// default color
CHSV getDefaultColor(int x, int y, int magnitude);
// fft mutex
Mutex fft_mux;

/**
 * LED Matrix Configuration. Provide the number of leds in x and y direction and
 * the data pin.
 * @author Phil Schatzmann
 */
struct LEDOutputConfig {
  /// Number of leds in x direction
  int x = 0;
  /// Number of leds in y direction
  int y = 1;
  /// optinal custom logic to provide CHSV color: Prividing a 'rainbow' color
  /// with hue 0-255, saturating 0-255, and brightness (value) 0-255 (v2)
  CHSV (*color_callback)(int x, int y, int magnitude) = getDefaultColor;
  /// Custom callback logic to update the LEDs - by default we use
  /// fftLEDOutput()
  void (*update_callback)(LEDOutputConfig *cfg, LEDOutput *matrix) = nullptr;
  /// Update the leds only ever nth call
  int update_frequency = 1; // update every call
  bool is_serpentine_layout = true;
  bool is_matrix_vertical = true;
  /// start bin which is displayed
  int fft_start_bin = 0;
  /// group result by adding subsequent bins
  int fft_group_bin = 1;
  /// Influences the senitivity
  int fft_max_magnitude = 700;
};

/**
 * LEDOutput using the FastLED library. You write the data to the FFT Stream.
 * This displays the result of the FFT to a LED matrix.
 * @ingroup io
 * @author Phil Schatzmann
 */
class LEDOutput {
public:
  LEDOutput() = default;

  /// @brief Default Constructor
  /// @param fft
  LEDOutput(AudioFFTBase &fft) {
    selfLEDOutput = this;
    p_fft = &fft;
    cfg.update_callback = fftLEDOutput;
  }

  LEDOutput(VolumePrint &vol) {
    selfLEDOutput = this;
    p_vol = &vol;
    cfg.update_callback = volumeLEDOutput;
  }

  /// Provides the default config object
  LEDOutputConfig defaultConfig() { return cfg; }

  /// Setup Led matrix
  bool begin(LEDOutputConfig config) {
    cfg = config;
    // if (!*p_fft) {
    //   LOGE("fft not started");
    //   return false;
    // }
    if (ledCount() == 0) {
      LOGE("x or y == 0");
      return false;
    }

    // allocate leds
    leds.resize(ledCount());
    for (int j = 0; j < ledCount(); j++) {
      led(j) = CRGB::Black;
    }

    // clear LED
    FastLED.clear(); // clear all pixel data

    if (p_fft != nullptr) {
      // assign fft callback
      AudioFFTConfig &fft_cfg = p_fft->config();
      fft_cfg.callback = fftCallback;

      // number of bins
      magnitudes.resize(p_fft->size());
      for (int j = 0; j < p_fft->size(); j++) {
        magnitudes[j] = 0;
      }
    }

    return true;
  }

  /// Provides the number of LEDs: call begin() first!
  int ledCount() {
    int num_leds = cfg.x * cfg.y;
    return num_leds;
  }

  ///  Provides the address fo the CRGB array: call begin() first!
  CRGB *ledData() {
    if (ledCount() == 0) {
      LOGE("x or y == 0");
      return nullptr;
    }
    // leds.resize(ledCount());
    return leds.data();
  }

  /// Updates the display: call this method in your loop
  virtual void update() {
    if (cfg.update_callback != nullptr && count++ % cfg.update_frequency == 0) {
      // use custom update logic defined in config
      cfg.update_callback(&cfg, this);
    }
  }

  /// Determine the led with the help of the x and y pos
  CRGB &ledXY(uint8_t x, uint8_t y) {
    if (x > cfg.x)
      x = cfg.x - 1;
    if (x < 0)
      x = 0;
    if (y > cfg.y)
      y = cfg.y - 1;
    if (y < 0)
      y = 0;
    int index = xy(x, y);
    return leds[index];
  }

  /// Determine the led with the help of the index pos
  CRGB &led(uint8_t index) {
    if (index > cfg.x * cfg.y)
      return not_valid;
    return leds[index];
  }

  /// Returns the magnitude for the indicated led x position. We might
  /// need to combine values from the magnitudes array if this is much bigger.
  virtual float getMagnitude(int x) {
    // get magnitude from fft
    float total = 0;
    for (int j = 0; j < cfg.fft_group_bin; j++) {
      int idx = cfg.fft_start_bin + (x * cfg.fft_group_bin) + j;
      if (idx >= magnitudes.size()) {
        idx = magnitudes.size() - 1;
      }
      total = magnitudes[idx];
    }
    return total / cfg.fft_group_bin;
  }

  /// @brief Provodes the max magnitude
  virtual float getMaxMagnitude() {
    // get magnitude from
    if (p_vol != nullptr) {
      return p_vol->volume();
    }
    float max = 0;
    for (int j = 0; j < cfg.x; j++) {
      float value = getMagnitude(j);
      if (value > max) {
        max = value;
      }
    }
    return max;
  }

  /// Update the indicated column with the indicated bar
  void updateColumn(int x, int currY) {
    // update vertical bar
    for (uint8_t y = 0; y < currY; y++) {
      // determine color
      CHSV color = cfg.color_callback(x, y, currY);
      // update LED
      ledXY(x, y) = color;
    }
    for (uint8_t y = currY; y < cfg.y; y++) {
      ledXY(x, y) = CRGB::Black;
    }
  }

  /// Update the last column with the indicated bar
  void updateColumn(int currY) { updateColumn(cfg.x - 1, currY); }

  /// Adds an empty column to the end shifting the content to the left
  void addEmptyColumn() {
    for (int x = 1; x < cfg.x; x++) {
      for (int y = 0; y < cfg.y; y++) {
        ledXY(x - 1, y) = ledXY(x, y);
      }
    }
    for (int y = 0; y < cfg.y; y++) {
      ledXY(cfg.x - 1, y) = CRGB::Black;
    }
  }

  /// Provides access to the actual config object. E.g. to change the update
  /// logic
  LEDOutputConfig &config() { return cfg; }

protected:
  friend class AudioFFTBase;
  CRGB not_valid;
  Vector<CRGB> leds{0};
  Vector<float> magnitudes{0};
  LEDOutputConfig cfg;
  AudioFFTBase *p_fft = nullptr;
  VolumePrint *p_vol = nullptr;
  uint64_t count = 0;



  uint16_t xy(uint8_t x, uint8_t y) {
    uint16_t i;

    if (cfg.is_serpentine_layout == false) {
      if (cfg.is_matrix_vertical == false) {
        i = (y * cfg.x) + x;
      } else {
        i = cfg.y * (cfg.x - (x + 1)) + y;
      }
    }

    if (cfg.is_serpentine_layout == true) {
      if (cfg.is_matrix_vertical == false) {
        if (y & 0x01) {
          // Odd rows run backwards
          uint8_t reverseX = (cfg.x - 1) - x;
          i = (y * cfg.x) + reverseX;
        } else {
          // Even rows run forwards
          i = (y * cfg.x) + x;
        }
      } else { // vertical positioning
        if (x & 0x01) {
          i = cfg.y * (cfg.x - (x + 1)) + y;
        } else {
          i = cfg.y * (cfg.x - x) - (y + 1);
        }
      }
    }

    return i;
  }

  /// callback method which provides updated data from fft
  static void fftCallback(AudioFFTBase &fft) {
    // just save magnitudes to be displayed
    LockGuard guard(fft_mux);
    for (int j = 0; j < fft.size(); j++) {
      float value = fft.magnitude(j);
      selfLEDOutput->magnitudes[j] = value;
    }
  };
};

/// Default update implementation which provides the fft result as "barchart"
void fftLEDOutput(LEDOutputConfig *cfg, LEDOutput *matrix) {
  {
    LockGuard guard(fft_mux);
    // process horizontal
    for (int x = 0; x < cfg->x; x++) {
      // max y determined by magnitude
      int currY = mapFloat(matrix->getMagnitude(x), 0, cfg->fft_max_magnitude,
                          0.0f, static_cast<float>(cfg->y));
      LOGD("x: %d, y: %d", x, currY);
      matrix->updateColumn(x, currY);
    }
  }
  FastLED.show();
}

/// Default update implementation which provides the fft result as "barchart"
void volumeLEDOutput(LEDOutputConfig *cfg, LEDOutput *matrix) {
  float vol = matrix->getMaxMagnitude();
  int currY = mapFloat(matrix->getMagnitude(vol), 0, cfg->fft_max_magnitude,
                        0.0f, static_cast<float>(cfg->y));
  matrix->addEmptyColumn();
  matrix->updateColumn(currY);
  FastLED.show();
}

/// @brief  Default logic to update the color for the indicated x,y position
CHSV getDefaultColor(int x, int y, int magnitude) {
  int color = map(magnitude, 0, 7, 255, 0);
  return CHSV(color, 255, 100); // blue CHSV(160, 255, 255
}

} // namespace audio_tools