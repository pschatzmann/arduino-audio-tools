/**
* @file streams-sd_m4a-audiokit.ino
* @author Peter Schatzmann
* @brief Example for decoding M4A files on the AudioKit using the AudioBoardStream
* @version 0.1
* @date 2023-10-01
*/

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecALAC.h"
#include "AudioTools/AudioCodecs/CodecAACHelix.h"
#include "AudioTools/AudioCodecs/ContainerM4A.h"
#include "AudioTools/AudioCodecs/MultiDecoder.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h" // install https://github.com/pschatzmann/arduino-audio-driver
#include "SD.h"

MultiDecoder multi_decoder;
ContainerM4A dec_m4a(multi_decoder);
AACDecoderHelix dec_aac;
DecoderALAC dec_alac;
AudioBoardStream out(AudioKitEs8388V1);
EncodedAudioOutput decoder_output(&out, &dec_m4a); 
File file;
StreamCopy copier(decoder_output, file);     

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // start AudioBoard with setup of CD pins
  auto cfg = out.defaultConfig(TX_MODE);
  cfg.sd_active = true;
  if (!out.begin(cfg)){
    Serial.println("Failed to start CSV output!");
    return;
  }

  if (!SD.begin(PIN_AUDIO_KIT_SD_CARD_CS)){
    Serial.println("SD Card initialization failed!");
    return;
  }

  file = SD.open("/m4a/aac.m4a");
  if (!file) {
    Serial.println("Failed to open file!");
    return;
  }

  // mp4 supports alac and aac
  multi_decoder.addDecoder(dec_alac,"audio/alac"); 
  multi_decoder.addDecoder(dec_aac,"audio/aac"); 

  // start decoder output
  if(!decoder_output.begin()) {
    Serial.println("Failed to start decoder output!");
    return;
  }

  Serial.println("M4A decoding started...");
}


void loop() { 
  copier.copy();
}