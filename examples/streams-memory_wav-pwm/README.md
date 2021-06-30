# Decoding a WAV file

In this example we decode a WAV file into RAW output and send it to a PWM pins (e.g. on a Raspberry Pico)
 
MemoryStream -> AudioOutputStream -> WAVDecoder -> AudioPWM

The pins depend on the Processor:

| PIEZO   |  ESP32         | RPI Pico        | 
| --------| ---------------|-----------------|
| +       |  GPIO4         | GPIO2           |
| -       |  GND           |                 |

