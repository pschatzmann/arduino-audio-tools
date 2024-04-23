# Stream Analog input to A2DP Bluetooth  

We can read an analog signal from a microphone and and send it to a Bluetooth A2DP device. To test the functionality I am using a MCP6022 microphone module.

![MCP6022](https://pschatzmann.github.io/Resources/img/mcp6022.jpeg)
![MCP6022](https://pschatzmann.github.io/Resources/img/mcp6022-1.jpeg)

The MCP6022 is a anlog microphone which operates at 3.3 V
We sample the sound signal with the help of the ESP32 I2S ADC input functionality.

### Pins:
 
| MCP6022 | ESP32
|---------|---------------
| VCC     | 3.3
| GND     | GND
| OUT     | GPIO34

### Dependencies

- https://github.com/pschatzmann/ESP32-A2DP.git


