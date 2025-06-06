/**
 * @file player-sd-i2s.ino
 * @brief example using the SD library that shows how to support m4a
 * aac, mp3 and alac files. We provid a MultiDecoder to the ContainerM4A
 * so that the container can decode aac and alac. We also provide it to 
 * the AudioPlayer so that we can play aac, alac, mp4 and m4a files. If
 * you only want to play m4a files, you can provide the ContainerM4A directly. 
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioTools/Disk/AudioSourceSD.h"
#include "AudioTools/AudioCodecs/CodecALAC.h"
#include "AudioTools/AudioCodecs/CodecAACHelix.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/AudioCodecs/ContainerM4A.h"
#include "AudioTools/AudioCodecs/MultiDecoder.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h" // install https://github.com/pschatzmann/arduino-audio-driver


const char *startFilePath="/m4a";
const char* ext="m4a";
AudioSourceSD source(startFilePath, ext);
AudioBoardStream i2s(AudioKitEs8388V1);  // or e.g. I2SStream i2s;
MultiDecoder multi_decoder;
ContainerM4A dec_m4a(multi_decoder);
AACDecoderHelix dec_aac;
MP3DecoderHelix dec_mp3;
DecoderALAC dec_alac;
AudioPlayer player(source, i2s, multi_decoder);


void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // setup multi decoder
  multi_decoder.addDecoder(dec_m4a, "audio/m4a");
  multi_decoder.addDecoder(dec_alac,"audio/alac"); 
  multi_decoder.addDecoder(dec_aac,"audio/aac"); 
  multi_decoder.addDecoder(dec_mp3,"audio/mp3"); 

  // setup output
  auto cfg = i2s.defaultConfig(TX_MODE);
  i2s.begin(cfg);

  // setup player
  //source.setFileFilter("*Bob Dylan*");
  player.begin();
}

void loop() {
  player.copy();
}