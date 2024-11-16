#pragma once
#include "Arduino_LED_Matrix.h"
#include "AudioTools/AudioLibs/AudioFFT.h"
#include "FFTDisplay.h"

namespace audio_tools {
class LEDOutputUnoR4;
struct LEDOutputUnoR4Config;

// default callback function which implements led update based on fft
void fftLEDOutputUnoR4(LEDOutputUnoR4Config *cfg, LEDOutputUnoR4 *matrix);
// led update for volume
void volumeLEDOutputUnoR4(LEDOutputUnoR4Config *cfg, LEDOutputUnoR4 *matrix);

/**
 * LED Matrix Configuration. Provide the number of leds in x and y direction and
 * the data pin.
 * @author Phil Schatzmann
 */
struct LEDOutputUnoR4Config {
  /// Custom callback logic to update the LEDs when update() is called
  void (*update_callback)(LEDOutputUnoR4Config *cfg,
                          LEDOutputUnoR4 *matrix) = nullptr;
  /// Update the leds only ever nth call
  int update_frequency = 1;  // update every call
  /// Number of LEDs in a rows
  int x = 12;
  /// Number of LEDs in a column
  int y = 8;
  /// when true 0,0 is in the lower left corder
  bool y_mirror = true;
  /// Influences the senitivity
  int max_magnitude = 700;
};

/**
 * @brief LED output using the R4 LED matrix library. 
 * @ingroup io
 * @author Phil Schatzmann
 */
class LEDOutputUnoR4 {

 public:
  /// @brief Default Constructor
  LEDOutputUnoR4() = default;

  /// @brief Constructor for FFT scenario
  /// @param fft
  LEDOutputUnoR4(FFTDisplay &fft) {
    p_fft = &fft;
    cfg.update_callback = fftLEDOutputUnoR4;
  }

  /// @brief Constructor for VolumeMeter scenario
  /// @param vol
  LEDOutputUnoR4(VolumeMeter &vol) {
    p_vol = &vol;
    cfg.update_callback = volumeLEDOutputUnoR4;
  }

  /// Provides the default config object
  LEDOutputUnoR4Config defaultConfig() { return cfg; }

  /// Starts the processing with the default configuration
  bool begin() { return begin(defaultConfig()); }

  /// Setup Led matrix
  bool begin(LEDOutputUnoR4Config config) {
    cfg = config;
    frame.resize(cfg.x * cfg.y);
    led_matrix.begin();
    max_column = -1;
    return true;
  }

  /// Updates the display by calling the update callback method: call this method in your loop
  virtual void update() {
    if (cfg.update_callback != nullptr && count++ % cfg.update_frequency == 0) {
      // use custom update logic defined in config
      cfg.update_callback(&cfg, this);
    } else {
      display();
    }
  }

  /// Determine the led with the help of the x and y pos
  bool &ledXY(uint8_t x, uint8_t y) {
    if (cfg.y_mirror) y = cfg.y - y - 1;
    return frame[x + (y * cfg.x)];
  }

  /// Provodes the max magnitude for the VolumeMeter and FFT scenario
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

  /// Update the indicated column with the indicated bar
  void setColumnBar(int x, int currY) {
    // update vertical bar
    for (uint8_t y = 0; y < currY; y++) {
      // update LED
      ledXY(x, y) = true;
    }
    for (uint8_t y = currY; y < cfg.y; y++) {
      ledXY(x, y) = false;
    }
    if (x > max_column) max_column = x;
  }

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
  LEDOutputUnoR4Config &config() { return cfg; }

  /// Update the led_matrix
  void display() {
    led_matrix.loadPixels((uint8_t *)frame.data(), cfg.x * cfg.y);
  }

  /// Provides access to the FFTDisplay object
  FFTDisplay& fftDisplay() {
    return *p_fft;
  }

 protected:
  friend class AudioFFTBase;
  LEDOutputUnoR4Config cfg;
  FFTDisplay *p_fft = nullptr;
  VolumeMeter *p_vol = nullptr;
  uint64_t count = 0;
  ArduinoLEDMatrix led_matrix;
  Vector<bool> frame{0};
  int max_column = -1;

  /// Adds an empty column to the end shifting the content to the left
  void addEmptyColumn() {
    for (int x = 1; x < cfg.x; x++) {
      for (int y = 0; y < cfg.y; y++) {
        ledXY(x - 1, y) = ledXY(x, y);
      }
    }
    for (int y = 0; y < cfg.y; y++) {
      ledXY(cfg.x - 1, y) = false;
    }
  }
};

/// Default update implementation which provides the fft result as "barchart"
void fftLEDOutputUnoR4(LEDOutputUnoR4Config *cfg, LEDOutputUnoR4 *matrix) {
  // process horizontal
  for (int x = 0; x < cfg->x; x++) {
    // max y determined by magnitude
    int currY = matrix->fftDisplay().getMagnitudeScaled(x, cfg->y);
    LOGD("x: %d, y: %d", x, currY);
    matrix->setColumnBar(x, currY);
  }
  matrix->display();
}

/// Default update implementation which provides the fft result as "barchart"
void volumeLEDOutputUnoR4(LEDOutputUnoR4Config *cfg, LEDOutputUnoR4 *matrix) {
  float vol = matrix->getMaxMagnitude();
  int currY = mapT<float>(vol, 0.0,
                       cfg->max_magnitude, 0.0f,
                       static_cast<float>(cfg->y));
  matrix->addColumnBar(currY);
  matrix->display();
}

}  // namespace audio_tools