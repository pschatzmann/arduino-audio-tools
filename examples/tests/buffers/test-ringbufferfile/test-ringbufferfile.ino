/**
 * @file test-ringbufferfile.ino
 * @author Phil Schatzmann
 * @brief genTest for the file backed ringbuffer
 * @version 0.1
 * @date 2023-04-30
 * 
 * @copyright Copyright (c) 2022
 * 
 */
/**
 * Test for the file backed ringbuffer
 */
#include "AudioTools.h"
#include <SdFat.h>
// SD pins
#define PIN_SD_CARD_CS 13
#define PIN_SD_CARD_MISO 2
#define PIN_SD_CARD_MOSI 15
#define PIN_SD_CARD_CLK 14

const char* file_name = "/tmp.bin";
SdFs SD;
FsFile file;
RingBufferFile<FsFile, int16_t> buffer(file);


void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // setup SD
  SPI.begin(PIN_SD_CARD_CLK, PIN_SD_CARD_MISO, PIN_SD_CARD_MOSI, PIN_SD_CARD_CS);
  while (!SD.begin(PIN_SD_CARD_CS, SPI_HALF_SPEED)) {
    Serial.println("Card Mount Failed");
    delay(500);
  }

  // create file and setup buffer
  file = SD.open(file_name, O_RDWR | O_CREAT );
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  
  // test write
  for (int j = 0; j < 10; j++) {
    buffer.write(j);
  }

  // test write array
  int16_t tmp[10];
  for (int j = 0; j < 10; j++) {
    tmp[j] = j;
  }
  buffer.writeArray(tmp, 10);

  // test read
  Serial.println("read");
  for (int j = 0; j < 10; j++) {
    int16_t result = buffer.read();
    Serial.print(result);
    Serial.print(" ");
    assert(result == j);
  }
  Serial.println();

  // test read array
  Serial.print("readArray");
  Serial.print(" ");
  Serial.println(buffer.available());
  memset(tmp, 0, 10 * sizeof(int16_t));
  
  int max = buffer.readArray(tmp, buffer.available());
  for (int j = 0; j < max; j++) {
    Serial.print(tmp[j]);
    Serial.print(" ");
    assert(j == tmp[j]);
  }
  Serial.println();

  file.close();
  Serial.println("Test success!");

  // cleanup
  SD.remove(file_name);
}

void loop() {}