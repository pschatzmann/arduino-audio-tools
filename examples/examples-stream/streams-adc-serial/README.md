
# Stream Analog Input to Serial Output

## General Description:

In this sketch we get the data from via the ESP32 I2S ADC and send it to the Plotter
Since the ADC via I2S is using the i2s port 0 with the pins GPIO34 and GPIO35.

The ACD provides values centered around 24576 but we want to make sure that the average of the singal is around 0, so we use 
the ConverterAutoCenter.

We can read an analog signal from a microphone. To test the functionality I am using a MCP6022 microphone module.

![MCP6022](https://pschatzmann.github.io/arduino-audio-tools/resources/mcp6022.jpeg)
![MCP6022](https://pschatzmann.github.io/arduino-audio-tools/resources/mcp6022-1.jpeg)

The MCP6022 is a anlog microphone which operates at 3.3 V
We sample the sound signal with the help of the ESP32 I2S ADC input functionality.


### Analog Input:

| ADC     |  ESP32
| --------| ---------------
| VCC     |  3.3V
| GND     |  GND
| OUT     |  GPIO34

Here is the result on the Arduino Serial Plotter:

![serial-plotter](https://pschatzmann.github.io/arduino-audio-tools/resources/serial-plotter-sine.png)


