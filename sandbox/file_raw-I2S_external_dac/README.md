# Stream SD File to I2S external DAC

We are reading a raw audio file from the SD card and write the data to the I2S interface. The audio file must be available using 16 bit integers with 2 channels. 

[Audacity](https://www.audacityteam.org/) might help you out here: export with the file name audio.raw as RAW signed 16 bit PCM and copy it to the SD card. In my example I was using the file [audio.raw](https://pschatzmann.github.io/arduino-audio-tools/resources/audio.raw). 

### SD Pins:

The SD module is connected with the help of the SPI bus

![sd](https://pschatzmann.github.io/arduino-audio-tools/resources/sd-module.jpeg)

We connect the SD to the ESP32:

| SD      | ESP32
|---------|---------------
| VCC     | 5V
| GND     | GND
| CS      | CS GP5
| SCK     | SCK GP18
| MOSI    | MOSI GP23
| MISO    | MISO GP19


### External DAC:
 
| DAC     |  ESP32
| --------| ---------------
| VDD     |  5V
| GND     |  GND
| SD      |  OUT (GPIO22)
| L/R     |  GND
| WS      |  WS (GPIO15)
| SCK     |  BCK (GPIO14)






