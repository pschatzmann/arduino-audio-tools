/**
 * @file streams-talkie-a2dp.ino
 * @author Phil Schatzmann
 * @copyright GPLv3
 * Using TalkiePCM to generate audio to be sent to a Bluetooth Speaker 
 */

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/AudioLibs/A2DPStream.h"
#include "TalkiePCM.h" // https://github.com/pschatzmann/TalkiePCM
#include "Vocab_US_Large.h"

const char* name = "LEXON MINO L";  // Replace with your device name

AudioInfo from(8000, 2, 16);  // TTS
AudioInfo to(44100, 2, 16);   // A2DP

A2DPStream a2dp;
FormatConverterStream out(a2dp);
// talkie is sumbmitting too many individual samples, so we buffer them
BufferedStream bs(1024, out); 
TalkiePCM voice(bs, from.channels);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  Serial.println("Starting...");

  // setup conversion to provide stereo at 44100hz
  out.begin(from, to);

  // setup a2dp
  auto cfg = a2dp.defaultConfig(TX_MODE);
  cfg.name = name;
  cfg.silence_on_nodata = true;  // allow delays with silence
  a2dp.begin(cfg);
  a2dp.setVolume(0.3);

  Serial.println("A2DP Started");
}

void loop() {
  voice.say(sp2_DANGER);
  voice.say(sp2_DANGER);
  voice.say(sp2_RED);
  voice.say(sp2_ALERT);
  voice.say(sp2_MOTOR);
  voice.say(sp2_IS);
  voice.say(sp2_ON);
  voice.say(sp2_FIRE);
  bs.flush();
  voice.silence(1000);
}
