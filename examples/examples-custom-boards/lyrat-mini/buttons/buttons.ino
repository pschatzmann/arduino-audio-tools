
#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

AudioBoardStream lyrat(LyratMini); 

void rec(bool, int, void*) {
   Serial.println("rec");
}

void mode(bool, int, void*) {
   Serial.println("mode");
}

void play(bool, int, void*) {
   Serial.println("play");
}

void set(bool, int, void*) {
   Serial.println("set");
}

void volUp(bool, int, void*) {
   Serial.println("vol+");
}

void volDown(bool, int, void*) {
   Serial.println("vol-");
}


// Arduino Setup
void setup(void) {
    Serial.begin(115200);
    AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

    // start board    
    lyrat.begin(lyrat.defaultConfig(TX_MODE));

    lyrat.addAction(KEY_REC, rec);
    lyrat.addAction(KEY_MODE, mode);
    lyrat.addAction(KEY_PLAY, play);
    lyrat.addAction(KEY_SET, set);
    lyrat.addAction(KEY_VOLUME_UP, volUp);
    lyrat.addAction(KEY_VOLUME_DOWN, volDown);

}

void loop() {
    lyrat.processActions();
}
