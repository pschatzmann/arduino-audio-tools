# A Simple Callback Audio Player

The example demonstrates how to implement an __MP3 Player__ which __provides the data via callbacks__ and sends the audio via I2S to an external DAC. 

This demonstrates the minimum implementation. I recommend however to provide implementations for the other callback methods to enhance the functionality: e.g absolute positioning, file selection by name etc. 


## SD Card

Here is the information how to wire the SD card to the ESP32

| SD    | ESP32 
|-------|-----------------------
| CS    | VSPI-CS0 (GPIO 05) 
| SCK   | VSPI-CLK (GPIO 18) 
| MOSI  | VSPI-MOSI (GPIO 23) 
| MISO  | VSPI-MISO (GPIO 19) 
| VCC   | VIN (5V) 
| GND   | GND 


### External DAC:

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



## Dependencies

- https://github.com/pschatzmann/arduino-audio-tools
- https://github.com/pschatzmann/arduino-libhelix
- https://github.com/greiman/SdFat
