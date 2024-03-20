
/**
 * We use the default Arduino SPI API to send/write the data as SPI Master.
 * 
 * Untested DRAFT implementation!
 */
#include <SPI.h>
#include "AudioTools.h"

const size_t BUFFER_SIZE = 1024;
const int SPI_CLOCK = 2000000;  // 2 MHz
AudioInfo info(44100, 2, 16); //
Vector<uint8_t> buffer(BUFFER_SIZE);
SineWaveGenerator<int16_t> sineWave(32000);
GeneratedSoundStream<int16_t> sound(sineWave);

void setup() {
  SPI.begin();
  sineWave.begin(info, N_B4);
}

void loop() {
  size_t result = sound.readBytes(&buffer[0], buffer.size());
  SPI.beginTransaction(SPISettings(SPI_CLOCK, MSBFIRST, SPI_MODE0));
  SPI.transfer(&buffer[0], result);
  SPI.endTransaction();
}
