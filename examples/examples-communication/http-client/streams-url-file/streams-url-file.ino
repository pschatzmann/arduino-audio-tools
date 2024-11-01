/**
 * @file streams-url-file.ino
 * @author Phil Schatzmann
 * @brief Demo how to download a file from the internet and store it to a file. The test was done with an AudioKit which requires
 * some specific pin settings
 * @version 0.1
 * @date 2022-09-09
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "SD.h"
#include "AudioTools.h"

#define PIN_AUDIO_KIT_SD_CARD_CS 13
#define PIN_AUDIO_KIT_SD_CARD_MISO 2
#define PIN_AUDIO_KIT_SD_CARD_MOSI 15
#define PIN_AUDIO_KIT_SD_CARD_CLK  14


const char *ssid = "SSID";
const char *password = "password";
URLStream url(ssid, password); // Music Stream
StreamCopy copier; //(i2s, music, 1024); // copy music to i2s
File file; // final output stream
int retryCount = 5;

// Arduino Setup
void setup(void) {
    Serial.begin(115200);
    AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

    // define custom SPI pins
    SPI.begin(PIN_AUDIO_KIT_SD_CARD_CLK, PIN_AUDIO_KIT_SD_CARD_MISO, PIN_AUDIO_KIT_SD_CARD_MOSI, PIN_AUDIO_KIT_SD_CARD_CS);

    // intialize SD
    if(!SD.begin(PIN_AUDIO_KIT_SD_CARD_CS)){   
        LOGE("SD failed");
        return;
    }

    // open music stream
    url.begin("https://pschatzmann.github.io/Resources/audio/audio-8000.raw");

    // copy file
    file = SD.open("/audio-8000.raw", FILE_WRITE);
    // overwirte from beginning
    file.seek(0); 
    // File returns avaiableForWrite() = 0, we we need to deactivate this check
    copier.setCheckAvailableForWrite(false);
    copier.begin(file, url);
    copier.copyAll();
    file.close();

    file = SD.open("/audio-8000.raw");
    LOGI("file size: %d", file.size());
}

void loop() {
}
