# Stream URL to I2S external DAC

We are reading a raw audio file from the Intenet and write the data to the I2S interface. The audio file must be available using 16 bit integers with 2 channels. I used a sampling rate of 8000.

[Audacity](https://www.audacityteam.org/) might help you out here: export with the file name audio.raw as RAW signed 16 bit PCM and copy it to the SD card. In my example I was using the file [audio.raw](https://pschatzmann.github.io/Resources/audio/audio.raw). 


### External DAC:

![DAC](https://pschatzmann.github.io/Resources/img/dac.jpeg)


I am just using the default pins defined by the framework. However I could change them with the help of the config object. The mute pin can be defined in the constructor of the I2SStream - by not defining anything we use the default which is GPIO23

DAC  |	ESP32
-----|----------------
VCC  |	5V
GND  |	GND
BCK  |	BCK (GPIO14)
DIN  |	OUT (GPIO22)
LCK  |	BCK (GPIO15)
FMT  |	GND
XMT  |	3V (or another GPIO PIN which is set to high)

- DMP - De-emphasis control for 44.1kHz sampling rate(1): Off (Low) / On (High)
- FLT - Filter select : Normal latency (Low) / Low latency (High)
- SCL - System clock input (probably SCL on your board).
- FMT - Audio format selection : I2S (Low) / Left justified (High)
- XMT - Soft mute control(1): Soft mute (Low) / soft un-mute (High)
