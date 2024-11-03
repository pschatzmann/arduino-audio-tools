
/**
 * @file sd.ino
 * @author Phil Schatzmann
 * @brief Test/Demo that SD is not working!
 * @version 0.1
 * @date 2024-11-03
 *
 * @copyright Copyright (c) 2022
 */

#include <SPI.h>
#include <SD.h>

// These pins are defined in the HAL
#define PIN_SD_CARD_CS 13  
#define PIN_SD_CARD_MISO 2
#define PIN_SD_CARD_MOSI 15
#define PIN_SD_CARD_CLK  14
#define PIN_SD_CARD_DET 34


// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  
  // setup SPI
  SPI.begin(PIN_SD_CARD_CLK, PIN_SD_CARD_MISO, PIN_SD_CARD_MOSI, PIN_SD_CARD_CS);

  // Optionally determine if there is an SD card
  pinMode(PIN_SD_CARD_DET, INPUT);
  if (digitalRead(PIN_SD_CARD_DET)!=0){
    Serial.println("No SD Card detected");
  }

  // Open SD library
  if (!SD.begin(PIN_SD_CARD_CS)){
    Serial.println("SD.begin failed");
    while(true);
  }

  // Open an existing file
  auto file = SD.open("/audio8000.raw", FILE_READ);
  if (!file){
    Serial.println("file open failed");
    while(true);
  }

  file.close();

  Serial.println("Success");
}

// Arduino loop - repeated processing 
void loop() {}