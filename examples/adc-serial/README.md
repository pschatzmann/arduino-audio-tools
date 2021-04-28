# Stream Analog input to Serial  

We can read an analog signal from a microphone and and write it out to Serial so that we can check the result in the Arduino Serial Plotter. To test the functionality I am using a MCP6022 microphone module.

![MCP6022](https://pschatzmann.github.io/arduino-sound-tools/resources/mcp6022.jpeg)
![MCP6022](https://pschatzmann.github.io/arduino-sound-tools/resources/mcp6022-1.jpeg)

The MCP6022 is a anlog microphone which operates at 3.3 V
We sample the sound signal with the help of the ESP32 I2S ADC input functionality.

### Pins:
 
| MCP6022 | ESP32
|---------|---------------
| VCC     | 3.3
| GND     | GND
| OUT     | GPIO34


