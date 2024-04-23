# A Simple SdFat Audio Player

The example demonstrates how to implement an __MP3 Player__: which provides the data from a SD drive and provides the audio via A2DP (e.g. to a Bluetooth Speaker): The __AudioSourceSdFat class__ builds on the [SdFat Library](https://github.com/greiman/SdFat) from Bill Greiman which provides FAT16/FAT32 and exFAT support. 

## SD Card

Here is the information how to wire the SD card to the ESP32

| SD    | ESP32 
|-------|-----------------------
| CS    | VSPI-CS0 (GPIO 05) 
| SCK   | VSPI-CLK (GPIO 18) 
| MOSI  | VSPI-MOSI (GPIO 23) 
| MISO  | VSPI-MISO (GPIO 19) 
| VCC   | VIN (5V) 
| GND   | GND 

![SD](https://www.pschatzmann.ch/wp-content/uploads/2021/04/sd-module.jpeg)




## Dependencies

- https://github.com/pschatzmann/arduino-audio-tools
- https://github.com/pschatzmann/arduino-libhelix
- https://github.com/greiman/SdFat
- https://github.com/pschatzmann/ESP32-A2DP
