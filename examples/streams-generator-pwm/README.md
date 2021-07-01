# PWM Output Test

This is a simple basic test for the PWM output.

We just send a generated sine wave to some defined pins and expect to hear some audio signal.

 
### Output Device: Piezo Electric Element

To test the output I am using a piezo electric element

![DAC](https://pschatzmann.github.io/arduino-audio-tools/resources/piezo.jpeg)

It should also be possible to connect a headphone to the output pins...


The pins depend on the Processor:


| PIEZO   |  ESP32       | RPI Pico      | MBED         |
| --------| -------------|---------------|--------------|
| +       |  GPI4/GPIO5  | GPI2/GPIO3    | GPI2/GPI3    |
| -       |  GND         |               |              |
