
# Stream I2S Input to CSV Output

## General Description:

We implement a I2S source: We stream the sound input which we read in from the I2S interface and displays it on the Arduino Serial Plotter. 

We can use any device which provides the sound data via I2S. In this case it is a 'I2S ADC Audio I2S Capture Card Module'


## Pins

![i2s-adc](https://pschatzmann.github.io/arduino-audio-tools/resources/I2S-ADC.jpg)


| i2s-ADC |  ESP32
| --------| ---------------
| VDD     |  3.3
| GND     |  GND
| SD      |  IN (GPIO32)
| WS      |  WS (GPIO15)
| SCK     |  BCK (GPIO14)


- SCK: Serial data clock for I²S interface
- WS: Select serial data words for the I²S interface
- L/R: Left / right channel selection
        When set to low, the microphone emits signals on the left channel of the I²S frame.
        When the high level is set, the microphone will send signals on the right channel.
- ExSD: Serial data output of the I²S interface
- VCC: input power 1.8V to 3.3V
- GND: Power groundHigh PSR: -75 dBFS.

