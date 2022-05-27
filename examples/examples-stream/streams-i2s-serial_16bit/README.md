
# Stream I2S Input to CSV Output

## General Description:

We implement a I2S source: We stream the sound input which we read in from the I2S interface and displays it on the Arduino Serial Plotter. 

We use only 16bits!

## Pins


| i2s-device |  ESP32
| -----------| ---------------
| BCK        |  BCK (GPIO14)
| WS         |  WS (GPIO15)
| DATA       |  IN (GPIO32)
| GND        |  GND
| VCC        |  VIN 5V


