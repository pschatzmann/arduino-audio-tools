
# Stream I2S Input to CSV Output

## General Description:

We implement a I2S source: We stream the sound input which we read in from the I2S interface and displays it on the Arduino Serial Plotter. 

We can use any device which provides the sound data via I2S. In this case it is a 'I2S ADC Audio I2S Capture Card Module'
Usually you do not need any master clock, but unfortunatly we need to feed this module with a master clock signal from the ESP32, if
the ESP32 is acting as master.

## Pins

![i2s-adc](https://pschatzmann.github.io/arduino-audio-tools/resources/I2S-ADC.jpg)


| i2s-ADC  |  ESP32
| ---------| ---------------
| MCCLK_IN |  RX_0 (GPIO3)
| BICK     |  BCK (GPIO14)
| DATA     |  IN (GPIO32)
| RLCLK    |  WS (GPIO15)
| GND      |  GND
| MUTE     |  -
| VCC      |  3.3

