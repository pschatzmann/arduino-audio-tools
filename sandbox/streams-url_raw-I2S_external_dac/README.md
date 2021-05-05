# Stream URL to I2S external DAC

We are reading a raw audio file from the Intenet and write the data to the I2S interface. The audio file must be available using 16 bit integers with 2 channels. 

[Audacity](https://www.audacityteam.org/) might help you out here: export with the file name audio.raw as RAW signed 16 bit PCM and copy it to the SD card. In my example I was using the file [audio.raw](https://pschatzmann.github.io/arduino-audio-tools/resources/audio.raw). 


### External DAC:
 
| DAC     |  ESP32
| --------| ---------------
| VDD     |  5V
| GND     |  GND
| SD      |  OUT (GPIO22)
| L/R     |  GND
| WS      |  WS (GPIO15)
| SCK     |  BCK (GPIO14)


Comments - The Playing is breaking up most likely because we receive the data not fast enough. Try to make an example with a lower sampes per second....



