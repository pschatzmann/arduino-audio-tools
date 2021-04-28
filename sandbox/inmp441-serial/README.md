
## Write Input from INMP441 Omnidirectional I2S Microphone Serial Output

![INMP441](https://pschatzmann.github.io/arduino-sound-tools/doc/resources/inmp441.jpeg)

### General Description:

We read the data from the I2S interface and write it to Serial

The INMP441 is a high-performance, low power, digital-output, omnidirectional MEMS microphone with a bottom port. The complete INMP441 solution consists of a MEMS sensor, signal conditioning, an analog-to-digital converter, anti-aliasing filters, power management, and an industry-standard 24-bit I²S interface. The I²S interface allows the INMP441 to connect directly to digital processors, such as DSPs and microcontrollers, without the need for an audio codec in the system.

### Pins:
 
| INMP441 | ESP32
|---------|---------------
| VCC     | 3.3
| GND     | GND
| SD      | IN (GPIO23)
| L/R     | -
| WS      | WS (GPIO25)
| SCK     | BCK (GPIO26)


- VCC: input power 1.8V to 3.3V
- GND: Power groundHigh PSR: -75 dBFS.
- ExSD: Serial data output of the I²S interface
- L/R: Left / right channel selection:
        When set to low, the microphone emits signals on the left channel of the I²S frame.
        When the high level is set, the microphone will send signals on the right channel.
- WS: Select serial data words for the I²S interface
- SCK: Serial data clock for I²S interface

