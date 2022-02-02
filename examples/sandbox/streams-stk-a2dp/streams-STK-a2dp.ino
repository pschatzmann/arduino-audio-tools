/**
 * @file streams-stk-a2dp.ino
 * @author Phil Schatzmann
 * @brief The Synthesis ToolKit in C++ (STK) is a set of open source audio signal processing and algorithmic synthesis classes written in the C++ programming language. 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

// Add this in your sketch or change the setting in AudioConfig.h
#define USE_A2DP
#define USE_STK

#include "AudioTools.h"
#include "Clarinet.h"



stk::Clarinet clarinet(440); // the stk clarinet instrument
STKGenerator<int16_t> generator(clarinet);               // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> in(generator);               // Stream generated from sine wave
A2DPStream a2dpStream = A2DPStream::instance(); // access A2DP as stream
StreamCopy copier(a2dpStream, in); // copy stkStream to a2dpStream
MusicalNotes notes; // notes with frequencies

// Arduino Setup
void setup(void) {
    Serial.begin(115200);
    AudioLogger::instance().begin(Serial, AudioLogger::Info);
    
    // start bluetooth
    Serial.println("starting A2DP...");
    auto cfgA2DP = a2dpStream.defaultConfig(TX_MODE);
    cfgA2DP.name = "LEXON MINO L";
    a2dpStream.setVolume(0.1);
    a2dpStream.begin(cfgA2DP);

    // start STK input with default configuration
    Serial.println("starting STK...");
    auto cfgSTK = in.defaultConfig();
    cfgSTK.channels = 2;
    in.setNotifyAudioChange(a2dpStream);
    in.begin(cfgSTK);

}

void playNoteStk() {
    static unsigned long timeout=0;
    static int note = 90;
    if (millis()>timeout) {
        note += rand() % 10 - 5; // generate some random offset
        float frequency = notes.frequency(note);
        clarinet.noteOn( frequency, 0.5 );

        // set time when we play next note
        timeout = millis()+1000;
    }
}

// Arduino loop - copy data
void loop() {
    playNoteStk();
    copier.copy();
}
