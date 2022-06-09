/**
 * @file streams-tts-i2s.ino
 * You need to install https://github.com/pschatzmann/arduino-simple-tts
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */

#include "SimpleTTS.h"
#include "AudioCodecs/CodecMP3Helix.h"
//#include "AudioLibs/AudioKit.h"


NumberToText ntt;
I2SStream out; // Replace with desired class e.g. AudioKitStream, AnalogAudioStream etc.
MP3DecoderHelix mp3;
AudioDictionary dictionary(ExampleAudioDictionaryValues);
TextToSpeech tts(ntt, out, mp3, dictionary);

int64_t number = 1;

void setup(){
    Serial.begin(115200);
    AudioLogger::instance().begin(Serial, AudioLogger::Warning);

    // setup out
    auto cfg = out.defaultConfig(); 
    cfg.sample_rate = 24000;
    cfg.channels = 1;
    out.begin(cfg);
}


void loop() {
    // speach output
    Serial.println("providing data...");
    ntt.say(number);

    number +=1;
    delay(1000);
}
