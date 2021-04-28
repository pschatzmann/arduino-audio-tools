# Arduino Audio Tools

Some basic C++ classes that can be used for Audio Processing using the Arduino Framework

- I2S (to read and write to the internal I2S) [ESP32 only]
- ADC (to read analog data with the help of I2S) [ESP32 only]
- Converters
- Notes (Frequencies of notes)
- SineWaveGenerator (to generate some sine tone)
- NBuffer (Multi buffer for writing and reading the data)
- TimerAlarmRepeating (e.g. for sampling sound data using exact times) [ESP32 only]

This functionality provides the glue which makes different sound processing components and libraries work together.
We also provide plenty of examples which demonstrate how to implement the different scenarios.

## Optional Libraries

- SD Library
- A2DP Library
- arduino-fdk-aac
- ESP8266Audio

## Project Status
This is currently work in progress. 


