# Decoding a WAV file

In this example we decode a WAV file into RAW output and let it output using the [Mozzi](https://sensorium.github.io/Mozzi/) output functionality.
Mozzi is supporting

- AVR
- ESP32
- ESP8266
- SAMD
- STM32
- Teensey3
- Raspberry Pico
- MBED

This flexibility comes at a price however:
- you need to configure the AUDIO_CHANNELS in the mozzi_config.h to the right value or provide the number of channels that are currently configured
- not all processors support all samping rates: it is recommended that you use the sampling rate which mozzi expects (this is available e.g. in AUDIO_RATE)

The WAV file has been down sampled with the help of __Audacity__ and then converted into an array with __xxd__. 

MemoryStream -> AudioOutputStream -> WAVDecoder -> MozziStream


## Preconditions

You need to have https://github.com/pschatzmann/Mozzi installed.
