/**
 * @file streams-tts-a2dp.ino
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */


#include "AudioTools.h"
#include "AudioCodecs/CodecMP3Helix.h"
#include "AudioLibs/A2DPStream.h"
#include "SimpleTTS.h"

const char* name = "LEXON MINO L";                         // Replace with your device name

AudioInfo from(24000, 1, 16);  // TTS                                         
AudioInfo to(44100, 2, 16);   // A2DP                                        

NumberToText ntt;
A2DPStream a2dp; 
FormatConverterStream out(a2dp);
MP3DecoderHelix mp3;
AudioDictionary dictionary(ExampleAudioDictionaryValues);
TextToSpeech tts(ntt, out, mp3, dictionary);

int64_t number = 1;

void setup(){
    Serial.begin(115200);
    AudioLogger::instance().begin(Serial, AudioLogger::Info);
    Serial.println("Starting...");

    // setup conversion to provide stereo at 44100hz
    out.begin(from, to);

    // setup a2dp
    auto cfg = a2dp.defaultConfig(TX_MODE); 
    cfg.name = name;
    cfg.silence_on_nodata = true; // allow delays with silence
    a2dp.begin(cfg);
    a2dp.setVolume(0.3);

    Serial.println("A2DP Started");

}


void loop() {
    // speach output
    Serial.print("Providing data: ");
    Serial.println(number);

    ntt.say(number);

    number +=1;
    delay(1000);
}
