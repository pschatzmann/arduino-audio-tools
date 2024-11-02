
// The following should work: but unforunately it does not work for me!

#include <SPI.h>
#include <SD.h>

// These pins are defined in the HAL
#define PIN_SD_CARD_CS 13  
#define PIN_SD_CARD_MISO 2
#define PIN_SD_CARD_MOSI 15
#define PIN_SD_CARD_CLK  14

// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  
  // setup SPI
  SPI.begin(PIN_SD_CARD_CLK, PIN_SD_CARD_MISO, PIN_SD_CARD_MOSI, PIN_SD_CARD_CLK);

  // Setup SD and open file
  if (!SD.begin(PIN_SD_CARD_CLK, SPI)){
    Serial.println("SD.begin failed");
    while(true);
  }

  auto file = SD.open("/audio8000.raw", FILE_READ);
  if (!file){
    Serial.println("file open failed");
    while(true);
  }

  Serial.println("Success");
}

// Arduino loop - repeated processing 
void loop() {
}