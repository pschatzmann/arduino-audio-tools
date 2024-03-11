## Micro Speech

The staring point for doing speech recognition on an Arduino based board is TensorFlow Light For Microcontrollers with the example sketch called micro_speech!

I have adapted the MicroSpeech example from TensorFlow Lite to follow the philosophy of this framework. The example uses a Tensorflow model which can recognise the words 'yes' and 'no'. The output stream class is TfLiteAudioOutput. In the example I am using an ESP32 AudioKit board, but you can replace this with any type of processor with a microphone.
Further information can be found in the [Wiki](https://github.com/pschatzmann/arduino-audio-tools/wiki/TensorFlow-Lite---MicroSpeech).
To capture the Audio we use an INMP441 Microphone:


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


- SCK: Serial data clock for I²S interface
- WS: Select serial data words for the I²S interface
- L/R: Left / right channel selection
        When set to low, the microphone emits signals on the left channel of the I²S frame.
        When the high level is set, the microphone will send signals on the right channel.
- ExSD: Serial data output of the I²S interface
- VCC: input power 1.8V to 3.3V
- GND: Power groundHigh PSR: -75 dBFS.



### Note

The log level has been set to Info to help you to identify any problems. Please change it to AudioLogger::Warning to get the best sound quality!


## Dependencies

You need to install the following libraries:

- https://github.com/pschatzmann/arduino-audio-tools
- https://github.com/pschatzmann/tflite-micro-arduino-examples
