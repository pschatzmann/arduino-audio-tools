/**
 * @file sdmmc.ino
 * @author Phil Schatzmann
 * @brief Test/Demo how to use the SD_MMC API in Arduino with the LyraT Mini
 * @version 0.1
 * @date 2024-11-03
 *
 * @copyright Copyright (c) 2022
 */

#include "FS.h"
#include "SD_MMC.h"

// These pins are defined in the HAL
const int PIN_SD_CARD_POWER = 13;  
const int PIN_SD_CARD_DET = 34;


// Arduino Setup
void setup(void) {
  Serial.begin(115200);

  // Mandatory: set power pin to low
  pinMode(PIN_SD_CARD_POWER, OUTPUT);
  digitalWrite(PIN_SD_CARD_POWER, LOW);

  // Optionally: Determine if there is an SD card
  pinMode(PIN_SD_CARD_DET, INPUT);
  if (digitalRead(PIN_SD_CARD_DET)!=0){
    Serial.println("No SD Card detected");
  }

  // open SDMMC in 1 bit mode
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("Card Mount Failed");
    while(true);
  }

  // open an existing file
  auto file = SD_MMC.open("/test.mp3", FILE_READ);
  if (!file){
    Serial.println("file open failed");
    while(true);
  }

  file.close();

  Serial.println("Success");
}

// Arduino loop - repeated processing 
void loop() {}