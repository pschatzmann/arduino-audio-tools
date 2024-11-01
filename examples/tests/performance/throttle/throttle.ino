#include "AudioTools.h"

AudioInfo info(44100, 2, 16);
SilenceGenerator<int16_t> silence;
GeneratedSoundStream<int16_t> sound(silence);
Throttle throttle(sound);
MeasuringStream out(200, &Serial);
StreamCopy copier(out, throttle);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  out.begin(info);
  sound.begin(info);
  throttle.begin(info);

}

void loop() { copier.copy(); }