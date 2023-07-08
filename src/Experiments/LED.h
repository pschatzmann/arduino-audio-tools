namespace audio_tools {

/**
 * Abstract LED api for updating an LED matrix. We provide an implementation for updating
 * the Arduino UNO R4 LED matrix and for FastLED
*/

struct LEDFunctions {
  virtual void clear();
  virtual void show();
};

struct LED {
  void setColor(Color* color){
};

struct Color {};

}
