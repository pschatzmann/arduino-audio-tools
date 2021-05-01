# Arduino Audio Tools

Some basic C++ classes that can be used for Audio Processing privided as Arduino Library

- Additional Stream implementations: MemoryStream - ESP32 only: UrlStream, I2SStream
- a simple I2S class (to read and write to the internal I2S) [ESP32 only]
- a simple ADC class (to read analog data with the help of I2S) [ESP32 only]
- Converters
- Musical Notes (with Frequencies of notes)
- SineWaveGenerator (to generate some sine tone)
- NBuffer (Multi buffer for writing and reading of (audio) data)
- TimerAlarmRepeating (e.g. for sampling audio data using exact times) [ESP32 only]
- A Wav Encoder and Decoder
- AudioOutputWithCallback class to provide callback integration with ESP8266Audio

This functionality provides the glue which makes different audio processing components and libraries work together.
We also provide plenty of examples that demonstrate how to implement the different scenarios. 

The design philosophy is based on the Arduino conventions: we use the ```begin()``` and ```end()``` methods to start and stop the processing and we propagate the use of Streams.


## Optional Libraries

Dependent on the example you might need to install some of the following libraries:

- [ESP32-A2DP Library](https://github.com/pschatzmann/ESP32-A2DP)
- [ESP8266Audio](https://github.com/earlephilhower/ESP8266Audio)
- [SD Library](https://www.arduino.cc/en/reference/SD)
- [arduino-fdk-aac](https://github.com/pschatzmann/arduino-fdk-aac)


# Examples

- [adc-a2dp](examples/adc-a2dp) - Stream Analog input to A2DP Bluetooth 
- [adc-serial](examples/adc-serial) - Stream Analog input to Serial
- [file_raw-a2dp](examples/file_raw-a2dp) - Stream Row File from SD card to A2DP Bluetooth
- [file_raw-serial](examples/file_raw-serial) - Stream Raw File from SD card to Serial
- [file_mp3-a2dp](examples/file_mp3-a2dp) - Stream MP3 File from SD card to A2DP Bluetooth using the ESP8266Audio library
- [i2s-a2dp](examples/i2s-a2dp) - Stream I2S Input to A2DP Bluetooth
- [i2s-serial](examples/i2s-serial) - Stream I2S Input to Serial


## Installation

You can download the library as zip and call include Library -> zip library. Or you can git clone this project into the Arduino libraries folder e.g. with

```
cd  ~/Documents/Arduino/libraries
git clone pschatzmann/arduino-audio-tools.git

```

## Documentation

Here is the generated [Class documentation](https://pschatzmann.github.io/arduino-audio-tools/html/annotated.html). You might find further information in [one of my blogs](https://www.pschatzmann.ch/home/category/machine-sound/)

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
| int24_t                | tested  |           

