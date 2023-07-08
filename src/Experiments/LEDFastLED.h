#include <FastLED.h>
#include "LED.h"

/**
 * LED API for the FastLED which is used by the LEDOutput
*/

namespace audio_tools {

struct FunctionsFastLED : public LEDFunctions {
  void clear() override { FastLED.clear(); }
  void show() override { FastLED.show(); }
};

struct ColorFastLED : public Color {
  ColorFastLED(CHSV color) { this->color = color; }
  CHSV color;
};

class LEDFastLED : public LED {
  void setColor(Color* color) { crgb = *(ColorFastLED*)color; }

 protected:
  CRGB crgb;
};

/// @brief  Default logic to update the color for the indicated x,y position
CHSV getDefaultColor(int x, int y, int magnitude) {
  int color = map(magnitude, 0, 7, 255, 0);
  return CHSV(color, 255, 100); // blue CHSV(160, 255, 255
}


}  // namespace audio_tools