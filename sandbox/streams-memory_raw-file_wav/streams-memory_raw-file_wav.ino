/**
 * @file aac_encode.ino
 * @author Phil Schatzmann
 * @brief Encode pwm data to AAC Stream (SD file)
 * @version 0.1
 * @date 2021-01-24
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#include <SPI.h>
#include <SD.h>
#include "WAVEncoder.h"
#include "StarWars30.h"
#include "AudioTools.h"

File dataFile = SD.open("example.wav", FILE_WRITE); 
WAVEncoder encoder(dataFile);
MemoryStream music(StarWars30_raw, StarWars30_raw_len);

void setup(){
    Serial.begin(115200);
    SD.begin(); 

    // setup encoder
    encoder.begin();
    Serial.println("Creating WAV file...");
}

void loop(){
    static uint8_t buffer[512];
    if (dataFile){
        if (music.available()>0){
            int len = music.readBytes(buffer, 512);
            encoder.write(buffer, len);
        } else {
            // no more data -> close file
            dataFile.close();
            Serial.println("File has been closed");
        }
    }
}