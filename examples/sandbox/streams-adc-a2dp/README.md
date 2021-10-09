# Stream Analog input to A2DP Bluetooth  

We can read an analog signal from a microphone and and send it to a __Bluetooth A2DP__ device (e.g. Bluetooth Speaker). To test the functionality I am using a MCP6022 microphone module.

![MCP6022](https://pschatzmann.github.io/arduino-audio-tools/resources/mcp6022.jpeg)
![MCP6022](https://pschatzmann.github.io/arduino-audio-tools/resources/mcp6022-1.jpeg)

The MCP6022 is a anlog microphone which operates at 3.3 V
We sample the sound signal with the help of the ESP32 I2S ADC input functionality.

In this implementation we are using the Streams !

### Supported Processors

This example works only on an ESP32.

### Dependencies

The [ESP32-A2DP](https://github.com/pschatzmann/ESP32-A2DP) Library must be installed.

### Pins:
 
| MCP6022 | ESP32
|---------|---------------
| VCC     | 3.3
| GND     | GND
| OUT     | GPIO34


