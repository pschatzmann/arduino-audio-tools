# Stream SD MP3 Files to A2DP Bluetooth using the ESP8266Audio Library


The processing of files with the help of the [ESP8266Audio](https://github.com/earlephilhower/ESP8266Audio) is a little bit more involved. Howver it allows to process different file types from different sources. Please consult the project for further details.

In this Sketch we use the ESP8266Audio library to read the [audio.mp3](https://pschatzmann.github.io/arduino-audio-tools/resources/audio.mp3) file from a SD drive. The output is pushed into a temporary buffer. The A2DP Callback then just consumes this buffered data... 

Please note that depending on the library used you might need to take care of some explicit pin assignments. E.g. on M5Stack

```
const int SPI_MOSI = 23;
const int SPI_MISO = 38;
const int SPI_SCK = 18;
const int SD_SS_PIN = 5;

SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

// Open MP3 file and play it
SD.begin(SD_SS_PIN);

```
