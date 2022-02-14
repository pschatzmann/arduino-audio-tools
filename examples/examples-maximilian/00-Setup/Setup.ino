

#include <FS.h>
#include <SD_MMC.h>
#include <WiFi.h>
#include "AudioTools.h"

/**
 * @brief Some examples are using files. Here is an example sketch for the ESP32 which creates
 * the files by copying the data from a URL.
 * 
 * @author pschatzmann
 */

const char* ssid = "ssid";
const char* password = "password";
StreamCopy copier; // copy kit to csvStream
URLStream url(ssid,password);

void downloadFile(const char *c_url, const char *path){
    Serial.print("processing ");
    Serial.println(path);
    
    auto file = SD_MMC.open(path, FILE_WRITE);
    url.begin(c_url);
    copier.begin(file, url);
    copier.copyAll();
    copier.end();
    url.end();
    file.flush();
    file.close();
}

void createDirectory() {
    // Create directory
    File root = SD_MMC.open("/");
    if (!root){
        if(SD_MMC.mkdir("/Maximilian")){
            Serial.println("Dir created");
        } else {
            Serial.println("mkdir failed");
        }
    }
    root.close();
}

void setup(){
    Serial.begin(115200);
    AudioLogger::instance().begin(Serial, AudioLogger::Warning);

    // setup SD
    if(!SD_MMC.begin()){
        Serial.println("Card Mount Failed");
        return;
    }

    createDirectory();

    downloadFile("https://pschatzmann.github.io/Maximilian/doc/resources/beat2.wav","/Maximilian/beat2.wav");
    downloadFile("https://pschatzmann.github.io/Maximilian/doc/resources/snare.wav","/Maximilian/snare.wav");
    downloadFile("https://pschatzmann.github.io/Maximilian/doc/resources/blip.wav","/Maximilian/blip.wav");
    downloadFile("https://pschatzmann.github.io/Maximilian/doc/resources/crebit2.wav","/Maximilian/crebit2.wav");
    downloadFile("https://pschatzmann.github.io/Maximilian/doc/resources/crebit2.ogg","/Maximilian/crebit2.ogg");

    Serial.println("The END");
}

void loop(){}