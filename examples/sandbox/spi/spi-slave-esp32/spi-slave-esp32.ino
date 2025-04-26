/***
 * We use the ESP32DMASPISlave library to implement the SPI slave on the ESP32
 * which receives the data
 * 
 * Untested DRAFT implementation!
 */
#include <ESP32DMASPISlave.h>
#include "AudioTools.h"

const size_t BUFFER_SIZE = 1024;
uint8_t *dma_rx_buf;
ESP32DMASPI::Slave spi_slave;
AudioInfo info(44100, 2, 16); //
I2SStream out; // or replace with another output

void setup() {
  Serial.begin(115200);
  // setup I2S
  auto cfg = out.defaultConfig(TX_MODE);
  cfg.copyFrom(info);
  //cfg.pin_bck = 14;
  //cfg.pin_ws = 15;
  //cfg.pin_data = 22;
  // to use DMA buffer, use these methods to allocate buffer
  dma_rx_buf = spi_slave.allocDMABuffer(BUFFER_SIZE);

  spi_slave.setDataMode(SPI_MODE0);           // default: SPI_MODE0
  spi_slave.setMaxTransferSize(BUFFER_SIZE);  // default: 4092 bytes

  // begin() after setting
  spi_slave.begin();  // default: HSPI (please refer README for pin assignments)

}

void loop() {
  // start and wait to complete one BIG transaction
  size_t received_bytes = spi_slave.transfer(nullptr, dma_rx_buf, BUFFER_SIZE);
  out.write(dma_rx_buf, received_bytes);
}
