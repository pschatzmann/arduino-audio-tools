/**
 * @file mp3-SynchronizedRingBuffer.ino
 * @author Phil Schatzmann
 * @brief Performance test for mp3 decoding form SD on core 1
 * @version 0.1
 * @date 2022-12-06
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "AudioTools.h"
#include "AudioLibs/AudioSourceSDFAT.h"
#include "AudioCodecs/CodecMP3Helix.h"
#include "AudioLibs/AudioBoardStream.h"

const char *startFilePath="/";
const char* ext="mp3";
AudioSourceSDFAT source(startFilePath, ext, PIN_AUDIO_KIT_SD_CARD_CS);
MP3DecoderHelix decoder;
MeasuringStream out;
AudioPlayer player(source, out, decoder);
int count=0;

void setup() {
 Serial.begin(115200);
 AudioLogger::instance().begin(Serial, AudioLogger::Warning);

 // sd_active is setting up SPI with the right SD pins by calling 
 SPI.begin(PIN_AUDIO_KIT_SD_CARD_CLK, PIN_AUDIO_KIT_SD_CARD_MISO, PIN_AUDIO_KIT_SD_CARD_MOSI, PIN_AUDIO_KIT_SD_CARD_CS);

 // setup player
 player.setDelayIfOutputFull(0);
 player.setVolume(0.1);
 player.begin(); 
}

void loop() {
    player.copy();
    count++;

    if (count==100){
        Serial.println(out.bytesPerSecond()/32);
        count = 0;
    }
}