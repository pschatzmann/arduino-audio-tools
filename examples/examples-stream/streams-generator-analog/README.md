# I2S Analog Output Test

This is a simple basic test for the ESP32 __analog output__ using I2S.

We just send a generated sine wave and expect to hear a clean signal.
Please note the log level should be set to Warning so that there is no disturbing output!

Make sure that you do not use any noisy power supply!

### Output Device: Earphones

You can also use some simple earphones

![DAC](https://pschatzmann.github.io/Resources/img/earphones.jpg)

On the ESP32 the stereo output is on the Pins GPIO25 and GPIO26

| Earphone|  ESP32           | UNO R4  |
| --------| -----------------|---------|
| Left    |  GPIO25          | A0      |
| Righ    |  GPIO25          | GND     |
| GND     |  GND             | GND     |


### Output Device: Piezo Electric Element

To test the output I am using a piezo electric element

![DAC](https://pschatzmann.github.io/Resources/img/piezo.jpeg)


On the ESP32 the stereo output is on the Pins GPIO25 and GPIO26

| PIEZO   |  ESP32           | UNO R4  |
| --------| -----------------|---------|
| +       |  GPIO25          | A0      |
| -       |  GND             | GND     |


