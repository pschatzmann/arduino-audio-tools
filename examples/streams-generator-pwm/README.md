# PWM Output Test

This is a simple basic test for the PWM output.

We just send a generated sine wave to some defined pins and expect to hear some audio signal.

 
### Output Device: Piezo Electric Element

To test the output I am using a piezo electric element

![DAC](https://pschatzmann.github.io/arduino-audio-tools/resources/piezo.jpeg)

It should also be possible to connect a headphone to the output pins...


The pins depend on the Processor:

| PIEZO   |  ESP32         | Rpi Pico        |
| --------| ---------------|-----------------|
| +       |  GPIO4/GPIO5   | GPIO02/GPIO03   |
| -       |  GND           |                 |