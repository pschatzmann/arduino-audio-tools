# Stream In Memory Midi to A2DP Bluetooth using the ESP8266Audio Library

The ESP32 can store only a limited amount of RAW data fully in the memory: Usually it's just sufficient for a couple of seconds. The memory wise most efficient file format is MIDI! By using MIDI we can easiy store one or even multiple pieces of music in the Flash Memory.  

We can play MIDI with the [ESP8266Audio](https://github.com/earlephilhower/ESP8266Audio) library.

And the nice thing: there is no need for a SD drive or any other additional hardware!

## Compile Settings

Set the Partition Scheme to Huge APP


## Warning
The quality is pretty bad ! 
Reason to be investigated...