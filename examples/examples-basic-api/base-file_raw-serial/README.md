# Stream SD File to A2DP Bluetooth

We are reading a raw audio file from the SD card and send it to a Bluetooth A2DP device. The audio file must be available using 16 bit integers with 2 channels.

[Audacity](https://www.audacityteam.org/) might help you out here: export with the file name audio.raw as RAW signed 16 bit PCM and copy it to the SD card. In my example I was using the file [audio.raw](https://pschatzmann.github.io/Resources/img/audio.raw). 

![sd](https://pschatzmann.github.io/Resources/img/sd-module.jpeg)

The SD module is connected with the help of the SPI bus

### Pins:

We connect the SD to the ESP32:

| SD      | ESP32
|---------|---------------
| VCC     | 5V
| GND     | GND
| CS      | CS GP5
| SCK     | SCK GP18
| MOSI    | MOSI GP23
| MISO    | MISO GP19


