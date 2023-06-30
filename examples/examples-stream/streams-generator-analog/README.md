# I2S Analog Output Test

This is a simple basic test for the ESP32 __analog output__ using I2S.

We just send a generated sine wave and expect to hear a clean signal.
Please note the log level should be set so that there is no disturbing output!

 
### Output Device: Piezo Electric Element

To test the output I am using a piezo electric element

![DAC](https://pschatzmann.github.io/Resources/img/piezo.jpeg)

You can also use some simple earphones

![DAC](https://pschatzmann.github.io/Resources/img/earphones.jpg)



On the ESP32 the output is on the Pins GPIO26 and GPIO27

| PIEZO   |  ESP32           | UNO R4  |
| --------| -----------------|---------|
| +       |  GPIO25 / GPIO26 | A0      |
| -       |  GND             | GND     |


