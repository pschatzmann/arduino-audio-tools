# Generating Audio with Tensorflow Liete

The starting point is the good overview provided by [the "Hallo World" example of Tensorflow Lite](https://www.tensorflow.org/lite/microcontrollers/get_started_low_level#train_a_model) which describes how to create, train and use a model which based on the __sine function__.

We just send a generated sine wave to the I2S interface. Further information can be found in the [Wiki](https://github.com/pschatzmann/arduino-audio-tools/wiki/Tensorflow-Lite----Audio-Output)
Please note the log level should be set so that there is no disturbing output!

 
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

You need to install the following libraries:

- https://github.com/pschatzmann/arduino-audio-tools
- https://github.com/pschatzmann/tflite-micro-arduino-examples