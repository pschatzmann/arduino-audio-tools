#pragma once
#include <FastLED.h>

#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/AudioLibs/AudioFFT.h"
#include "FFTDisplay.h"

namespace audio_tools {

class LEDOutput;
struct LEDOutputConfig;

// default callback function which implements led update
void fftLEDOutput(LEDOutputConfig *cfg, LEDOutput *matrix);
// led update for volume
void volumeLEDOutput(LEDOutputConfig *cfg, LEDOutput *matrix);
// default color
CHSV getDefaultColor(int x, int y, int magnitude);

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
  int update_frequency = 1;  // update every call
  bool is_serpentine_layout = true;
  bool is_matrix_vertical = true;
  /// Influences the senitivity
  int max_magnitude = 700;
};

/**
 * @brief LED output using the FastLED library. 
 * @author Phil Schatzmann
 */
class LEDOutput {
 public:
  /// @brief Default Constructor
  LEDOutput() = default;

  /// @brief Constructor for FFT scenario
  /// @param fft
  LEDOutput(FFTDisplay &fft) {
    p_fft = &fft;
    cfg.update_callback = fftLEDOutput;
  }

  /// @brief Constructor for VolumeMeter scenario
  /// @param vol
  LEDOutput(VolumeMeter &vol) {
    p_vol = &vol;
    cfg.update_callback = volumeLEDOutput;
  }

  /// Provides the default config object
  LEDOutputConfig defaultConfig() { return cfg; }

  /// Setup Led matrix
  bool begin(LEDOutputConfig config) {
    cfg = config;
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
    FastLED.clear();  // clear all pixel data

    if (p_fft != nullptr) {
      p_fft->begin();
    }

    max_column = -1;

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
    } else {
      display();
    }
  }

  /// Determine the led with the help of the x and y pos
  CRGB &ledXY(uint8_t x, uint8_t y) {
    if (x > cfg.x) x = cfg.x - 1;
    if (x < 0) x = 0;
    if (y > cfg.y) y = cfg.y - 1;
    if (y < 0) y = 0;
    int index = xy(x, y);
    return leds[index];
  }

  /// Determine the led with the help of the index pos
  CRGB &led(uint8_t index) {
    if (index > cfg.x * cfg.y) return not_valid;
    return leds[index];
  }

  /// Update the indicated column with the indicated bar
  void setColumnBar(int x, int currY) {
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
    if (x > max_column) max_column = x;
  }

  /// Update the last column with the indicated bar
  void setColumnBar(int currY) { setColumnBar(cfg.x - 1, currY); }

  /// Update the last column with the indicated bar
  void addColumnBar(int currY) {
    max_column++;
    if (max_column >= cfg.x) {
      addEmptyColumn();
    }
    if (max_column > cfg.x - 1) {
      max_column = cfg.x - 1;
    }
    setColumnBar(max_column, currY);
  }

  /// Provides access to the actual config object. E.g. to change the update logic
  LEDOutputConfig &config() { return cfg; }

  ///Provodes the max magnitude for both the 
  virtual float getMaxMagnitude() {
    // get magnitude from
    if (p_vol != nullptr) {
      return p_vol->volume();
    }
    float max = 0;
    if (p_fft != nullptr) {
      for (int j = 0; j < cfg.x; j++) {
        float value = p_fft->getMagnitude(j);
        if (value > max) {
          max = value;
        }
      }
    }
    return max;
  }

  /// Update the led_matrix (calling FastLED.show();
  void display() {
    FastLED.show();
  }

  /// Provides acces to the FFTDisplay object
  FFTDisplay &fftDisplay() { return *p_fft; }

 protected:
  friend class AudioFFTBase;
  CRGB not_valid;
  Vector<CRGB> leds{0};
  LEDOutputConfig cfg;
  VolumeMeter *p_vol = nullptr;
  FFTDisplay *p_fft = nullptr;
  uint64_t count = 0;
  int max_column = -1;

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
      } else {  // vertical positioning
        if (x & 0x01) {
          i = cfg.y * (cfg.x - (x + 1)) + y;
        } else {
          i = cfg.y * (cfg.x - x) - (y + 1);
        }
      }
    }

    return i;
  }
};

/// Default update implementation which provides the fft result as "barchart"
void fftLEDOutput(LEDOutputConfig *cfg, LEDOutput *matrix) {
  // process horizontal
  LockGuard guard(fft_mux);
  for (int x = 0; x < cfg->x; x++) {
    // max y determined by magnitude
    int currY = matrix->fftDisplay().getMagnitudeScaled(x, cfg->y);
    LOGD("x: %d, y: %d", x, currY);
    matrix->setColumnBar(x, currY);
  }
  FastLED.show();
}

/// Default update implementation which provides the fft result as "barchart"
void volumeLEDOutput(LEDOutputConfig *cfg, LEDOutput *matrix) {
  float vol = matrix->getMaxMagnitude();
  int currY = mapT<float>(vol, 0,
                       cfg->max_magnitude, 0.0f,
                       static_cast<float>(cfg->y));
  matrix->addColumnBar(currY);
  FastLED.show();
}

/// Default logic to update the color for the indicated x,y position
CHSV getDefaultColor(int x, int y, int magnitude) {
  int color = map(magnitude, 0, 7, 255, 0);
  return CHSV(color, 255, 100);  // blue CHSV(160, 255, 255
}

}  // namespace audio_tools