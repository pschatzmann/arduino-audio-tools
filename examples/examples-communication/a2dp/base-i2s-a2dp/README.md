
# Stream I2S Input to A2DP Bluetooth

## General Description:
We implement a A2DP source: We stream the sound input which we read in from the I2S interface to a A2DP sink. We can use any device which provides the sound data via I2S. In order to test the functionality we use the INMP441 microphone.

![INMP441](https://pschatzmann.github.io/Resources/img/inmp441.jpeg)

The INMP441 is a high-performance, low power, digital-output, omnidirectional MEMS microphone with a bottom port. The complete INMP441 solution consists of a MEMS sensor, signal conditioning, an analog-to-digital converter, anti-aliasing filters, power management, and an industry-standard 24-bit I²S interface. The I²S interface allows the INMP441 to connect directly to digital processors, such as DSPs and microcontrollers, without the need for an audio codec in the system.

## Pins
 
| INMP441 |  ESP32
| --------| ---------------
| VDD     |  3.3
| GND     |  GND
| SD      |  IN (GPIO32)
| L/R     |  GND
| WS      |  WS (GPIO15)
| SCK     |  BCK (GPIO14)


SCK: Serial data clock for I²S interface
WS: Select serial data words for the I²S interface
L/R: Left / right channel selection
        When set to low, the microphone emits signals on one channel of the I²S frame.
        When the high level is set, the microphone will send signals on the other channel.
ExSD: Serial data output of the I²S interface
VCC: input power 1.8V to 3.3V
GND: Power groundHigh PSR: -75 dBFS.

### Dependencies

- https://github.com/pschatzmann/ESP32-A2DP.git

