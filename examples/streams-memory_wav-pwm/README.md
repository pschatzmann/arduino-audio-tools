# Decoding a WAV file

In this example we decode a WAV file into RAW output and send it to a PWM pins (e.g. on a Raspberry Pico)
 
MemoryStream -> AudioOutputStream -> WAVDecoder -> AudioPWM

The pins depend on the Processor:

| PIEZO   |  ESP32         | AVR (Nano)      | Rpi Pico
| --------| ---------------|-----------------|--------------
| +       |  GPIO3 / GPIO4 | GPIO3 / GPIO11  | GPIO03/GPIO04
| -       |  GND           |                 |

