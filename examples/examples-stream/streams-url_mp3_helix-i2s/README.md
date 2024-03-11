# Streaming Radio Player

We just play a streamed radio station which provides the audio as mp3 and output the result via I2S to an external DAC
To decode the data we use the libhelix library.

An ESP32 was used to test this sketch.

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

### Dependencies

- https://github.com/pschatzmann/arduino-libhelix


