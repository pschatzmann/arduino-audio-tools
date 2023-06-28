
# Stream PDM Input to CSV Output

## General Description:

To use a PDM microphone you need to have a microcontroller that supports PDM input or a dedicated library.

The ESP32 supports PDM via the I2S API: We stream the sound input from a PDM microphone (PDM Digital MEMS MP34DT01) which we read in from the I2S interface and displays it on the Arduino Serial Plotter. 


## Pins

![i2s-adc](https://pschatzmann.github.io/Resources/img/pdm-mic.jpg)

| i2s-mic  |  ESP32
| ---------| ---------------
| 3V       |  3V
| GND      |  GND
| SEL      |  GND  (GND or 3.3V)
| CLK      |  WS (GPIO15)
| DAT      |  IN (GPIO32)
| -        |  BCK (GPIO14)


## Additional Comments

You can select if you receive only data on the left or right biy setting SEL to high or low.
Please note that the WS is connected to the CLK of the microphone!

