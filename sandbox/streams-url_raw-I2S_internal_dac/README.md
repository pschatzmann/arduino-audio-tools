# Stream URL to I2S internal DAC

I am reading a raw audio file from the Intenet and write the data to the analog pins of the ESP32 using the I2S interface. The audio file must be available using 16 bit integers with 2 channels. 

[Audacity](https://www.audacityteam.org/) might help you out here: export with the file name audio.raw as RAW signed 16 bit PCM and copy it to the SD card. In my example I was using the file [audio.raw](https://pschatzmann.github.io/arduino-audio-tools/resources/audio.raw). 


### Amplifier Pins:

To hear the sound I connected the ESP32 to an amplifier module: The analog output is available on GPIO25 & GPIO26. But you could also use some earphones...


| Amp     | ESP32
|---------|---------------
| +       | 5V
| -       | GND
| L       | GPIO25
| R       | GPIO26
| T       | GND





