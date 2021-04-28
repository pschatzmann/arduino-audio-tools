# Arduino Audio Tools

Some basic C++ classes that can be used for Audio Processing privided as Arduino Library

- a simple I2S class (to read and write to the internal I2S) [ESP32 only]
- a simple ADC class (to read analog data with the help of I2S) [ESP32 only]
- Converters
- Notes (Frequencies of notes)
- SineWaveGenerator (to generate some sine tone)
- NBuffer (Multi buffer for writing and reading of (sound) data)
- TimerAlarmRepeating (e.g. for sampling sound data using exact times) [ESP32 only]

This functionality provides the glue which makes different sound processing components and libraries work together.
We also provide plenty of examples that demonstrate how to implement the different scenarios.

## Optional Libraries

- SD Library
- A2DP Library
- arduino-fdk-aac
- ESP8266Audio

## Examples

All examples can be found in the [examples folder](https://github.com/pschatzmann/arduino-sound-tools/tree/main/examples)

## Documentation

Here is the generated [Class documentation](https://pschatzmann.github.io/arduino-sound-tools/html/annotated.html)

## Project Status

This is currently work in progress. 


