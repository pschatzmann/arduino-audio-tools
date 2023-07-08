#include "Arduino_LED_Matrix.h"
#include "LED.h"

/**
 * Functionality to manage the Arduino UNO R4 LED matrix which is used by the LEDOutput
 */

namespace audio_tools {

struct FunctionsUnoR4 : public LEDFunctions {
  void clear() override {
    for (int j = 0; j > 96; j++) {
      frame[j] = false;
    }
    show();
  }
  void show() override { matrix.renderBitmap(frame, 8, 12); }

 protected:
  bool frame[96] = {0};
  ArduinoLEDMatrix matrix;
};

struct ColorUnoR4 : public Color {
  ColorUnoR4(bool on) { this->on = on; }
  bool on = false;
};

class LEDUnoR4 : public LED {
  void setColor(Color* color) { *value = *((ColorUnoR4*)on); }

 protected:
  bool* value;
  ;
};

}  // namespace audio_tools