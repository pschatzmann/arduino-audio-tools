# Arduino Audio Tools

Some basic C++ classes that can be used for Audio Processing privided as Arduino Library

- Additinal Stream implementations: MemoryStream, UrlStream [ESP32 only]
- a simple I2S class (to read and write to the internal I2S) [ESP32 only]
- a simple ADC class (to read analog data with the help of I2S) [ESP32 only]
- Converters
- Musical Notes (with Frequencies of notes)
- SineWaveGenerator (to generate some sine tone)
- NBuffer (Multi buffer for writing and reading of (sound) data)
- TimerAlarmRepeating (e.g. for sampling sound data using exact times) [ESP32 only]
- A Wav Encoder and Decoder

This functionality provides the glue which makes different sound processing components and libraries work together.
We also provide plenty of examples that demonstrate how to implement the different scenarios.

## Optional Libraries

- [ESP32-A2DP Library](https://github.com/pschatzmann/ESP32-A2DP)
- [arduino-fdk-aac](https://github.com/pschatzmann/arduino-fdk-aac)
- [ESP8266Audio](https://github.com/earlephilhower/ESP8266Audio)
- [SD Library](https://www.arduino.cc/en/reference/SD)

## Examples

All examples can be found in the [examples folder](https://github.com/pschatzmann/arduino-sound-tools/tree/main/examples)


## Installation

You can download the library as zip and call include Library -> zip library. Or you can git clone this project into the Arduino libraries folder e.g. with

```
cd  ~/Documents/Arduino/libraries
git clone pschatzmann/arduino-sound-tools.git

```

## Documentation

Here is the generated [Class documentation](https://pschatzmann.github.io/arduino-sound-tools/html/annotated.html). You might find further information in [one of my blogs](https://www.pschatzmann.ch/home/category/machine-sound/)

## Project Status

This is currently work in progress:

| Functionality          | Status  |
|------------------------|---------|
| Analog input - ADC     | tested  |
| I2S                    | tested  |
| Files (RAW, MP3...)    | tested  |
| Streams                | open    |
| WAV encoding/deconding | open    |          
| AAC encoding/deconding | open    |          
| int24_t                | open    |           

