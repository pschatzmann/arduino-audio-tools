
# Stream Analog Input to Serial Output

## General Description:

In this sketch we get the data from via the ESP32 I2S ADC and send it to the Plotter
Since the ADC via I2S is using the i2s port 0 with the pins GPIO34 and GPIO35.

The ACD provides values centered around 24576 but we want to make sure that the average of the singal is around 0, so we use the ConverterAutoCenter.

### Analog Input:

| ADC     |  ESP32
| --------| ---------------
| GND     |  GND
| Left    |  GPIO34
| Right   |  GPIO35


