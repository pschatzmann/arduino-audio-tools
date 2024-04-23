# A Simple Icecast Streaming Audio Player

Compared to the regular URLStream, and ICYStream provides audio Metadata.

### The External DAC

For my tests I am using the 24-bit PCM5102 PCM5102A Stereo DAC Digital-to-analog Converter PLL Voice Module pHAT

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


### Wiring the Volume Potentiometer

![DAC](https://www.pschatzmann.ch/wp-content/uploads/2021/10/Pot.jpg)

| Pot     |  ESP32   | ESP8266
| --------| ---------|---------
| POW     |  3V      | 3V
| GND     |  GND     | GND
| VOUT    |  A0      | A0

### Moving to the next song

We use a button to move to the next url. 

|  Button    |  ESP32   | ESP8266
|------------|----------|---------
|  Button    |  GPIO13  | GPIO13
|  Out 3V


### Dependencies

- https://github.com/pschatzmann/arduino-audio-tools
- https://github.com/pschatzmann/arduino-libhelix
