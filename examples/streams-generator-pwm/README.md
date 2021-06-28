# PWM Output Test

This is a simple basic test for the PWM output.

We just send a generated sine wave to some defined pins and expect to hear some audio signal.

 
### Output Device: Piezo Electric Element

To test the output I am using a piezo electric element

![DAC](https://pschatzmann.github.io/arduino-audio-tools/resources/piezo.jpeg)

It should also be possible to connect a headphone to the output pins...


On the ESP32 the output is on the Pins GPIO26 and GPIO27

| PIEZO     |  ESP32
| --------| ---------------
| +       |  GPIO22 / GPIO23
| -       |  GND