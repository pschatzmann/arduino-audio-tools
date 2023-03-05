# Decoding a MP3 file

In this example we decode a MP3 file into RAW output and write the output with the help of a PWM signal to the pins. 

Currently we provide multiple mp3 decoders
- libhelix 
- tinymp3

Unfortunatly non of those are currently stable in all environments....


### Output Device: Earphone

To test the output I am using an earphone from a mobile phone. 
We get 2 signals one for the left and the other for the right channel.

![DAC](https://pschatzmann.github.io/Resources/img/earphones.jpg)


The pins depend on the Processor:


| EarPhone   |  ESP32       | RPI Pico      | MBED         |
| -----------| -------------|---------------|--------------|
| Left       |  GPI2        | GPI2          | GPI2         |
| Right      |  GPI3        | GPIO3         | GPI3         |
| GND        |  GND         | GND           | GND          |

To verify check the PIN_PWM_START in AudioConfig.h or you can set the pins by calling setPins() method on the PWMConfig object.