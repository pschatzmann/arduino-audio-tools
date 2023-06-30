# PWM Output Test

This is a simple basic test for the PWM output.

We just send a generated sine wave to some defined pins and expect to hear some audio signal.

 
### Output Device: Piezo Electric Element

To test the output I am using a piezo electric element

![DAC](https://pschatzmann.github.io/Resources/img/piezo.jpeg)

You can also use some simple earphones

![DAC](https://pschatzmann.github.io/Resources/img/earphones.jpg)


It should also be possible to connect a headphone to the output pins...


The pins depend on the Processor:


| PIEZO   |  ESP32       | RPI Pico      | MBED         | UNO R4      |
| --------| -------------|---------------|--------------|-------------|
| +       |  GPIO4/GPIO5 | GPIO2/GPIO3   | GPIO2/GPIO3  | GPIO2/GPIO4 |
| -       |  GND         | GND           | GND          | GND         |
