
/**
 * @file web-radio.ino
 * @brief Web radio example for the Guition JC4880P443C_I_W (ESP32-P4) board
 * with onboard ES8311 codec + NS4150 speaker amp, via  I2SCodecStream and
 * EncodedAudioStream. This is a test if the wifi on this board is fast enough
 * to stream mp3 audio from the internet. We use the TinyGPU library to
 * determine the pins.
 *
 * The speed seems to be sufficient, but I noticed it bo be quite instable and
 * to hang the board after a while. I suspect that the wifi is not stable enough
 * on this board.
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/Communication/AudioHttp.h"
#include "TinyGPU.h"
#include "TinyGPU/LCDBoardsESP32.h"

URLStream url("ssid", "password");
I2SCodecStream i2s(GenericES8311);  // final output of decoded stream
EncodedAudioStream dec(&i2s, new MP3DecoderHelix());  // Decoding stream
StreamCopy copier(dec, url);                          // copy url to decoder
JC4880P443C_I_W board;

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  // AudioDriverLogger.begin(Serial,AudioDriverLogLevel::Debug);

  while (!Serial) delay(50);

  if (!board.begin()) {
    Serial.println("baord begin failed");
    stop();
  }

  // setup i2s
  auto config = i2s.defaultConfig(TX_MODE);
  board.setI2SPins<I2SConfig>(config);
  if (!i2s.begin(config)) {
    Serial.println("I2S begin failed");
    stop();
  }
  i2s.setVolume(0.4);

  // Activate amplifier
  board.setAmplifierActive(true);

  // setup decoer: sampling rate provided by decoder
  if (!dec.begin()) {
    Serial.println("failed to start dec");
    stop();
  }

  // mp3 radio
  if (!url.begin("http://stream.srg-ssr.ch/m/rsj/mp3_128", "audio/mp3")) {
    Serial.print("url begin failed");
    stop();
  }
}

void loop() { copier.copy(); }
