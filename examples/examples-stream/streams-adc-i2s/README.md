
# Stream Analog Input to I2S

## General Description:

In this sketch we get the data from via the ESP32 I2S ADC and send it to I2S
Since the ADC via I2S is using the i2s port 0, we use port 1 for the output.

### Analog Input:

| ADC     |  ESP32
| --------| ---------------
| VCC     |  3.3V
| GND     |  GND
| OUT     |  GPIO34

For the input I was using a MCP6022 Microphone Sensor.
Plaese note that the signal that we receive from the ADC needs to be adjusted so that it is oscillating around 0.

![MCP6022](https://pschatzmann.github.io/Resources/img/mcp6022.jpeg)


### External DAC:


For my tests I am using the 24-bit PCM5102 PCM5102A Stereo DAC Digital-to-analog Converter PLL Voice Module pHAT

I am just using the default pins defined by the framework. However I could change them with the help of the config object. The mute pin can be defined in the constructor of the I2SStream - by not defining anything we use the default which is GPIO23

DAC  |	ESP32
-----|----------------
VCC  |	5V
GND  |	GND
BCK  |	BCK (GPIO14)
DIN  |	OUT (GPIO22)
LCK  |	WS (GPIO15)
FMT  |	GND
XMT  |	3V (or another GPIO PIN which is set to high)

- DMP - De-emphasis control for 44.1kHz sampling rate(1): Off (Low) / On (High)
- FLT - Filter select : Normal latency (Low) / Low latency (High)
- SCL - System clock input (probably SCL on your board).
- FMT - Audio format selection : I2S (Low) / Left justified (High)
- XMT - Soft mute control(1): Soft mute (Low) / soft un-mute (High)

![DAC](https://pschatzmann.github.io/Resources/img/dac.jpeg)
