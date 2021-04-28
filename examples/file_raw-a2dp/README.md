# Stream SD File to A2DP Bluetooth

We are reading a raw audio file from the SD card and send it to a Bluetooth A2DP device. The audio file must be available using 16 bit integers with 2 channels. 

[Audacity](https://www.audacityteam.org/) might help you out here: export with the file name audio.raw as RAW signed 16 bit PCM and copy it to the SD card.

![sd](https://pschatzmann.github.io/arduino-sound-tools/resources/sd-module.jpeg)

The SD module is connected with the help of the SPI bus

### Pins:

We use SPI2 (HSPI):

| MCP6022 | ESP32
|---------|---------------
| VCC     | 5V
| GND     | GND
| CS      | CS GP15
| SCK     | SCK GP14
| MOSI    | MOSI GP13
| MISO    | MISO GP12


