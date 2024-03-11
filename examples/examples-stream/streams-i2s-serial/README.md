
# Stream I2S Input to CSV Output

## General Description:

We implement a I2S source: We stream the sound input which we read in from the I2S interface and displays it on the Arduino Serial Plotter. 

We can use any device which provides the sound data via I2S. In this case it is a 'I2S ADC Audio I2S Capture Card Module'
Usually you do not need any master clock, but unfortunatly we need to feed this module with a master clock signal from the ESP32, if
the ESP32 is acting as master.

## Pins

![i2s-adc](https://pschatzmann.github.io/Resources/img/I2S-ADC.jpg)


| i2s-ADC  |  ESP32
| ---------| ---------------
| MCCLK_IN |  RX_0 (GPIO3)
| BICK     |  BCK (GPIO14)
| DATA     |  IN (GPIO32)
| RLCLK    |  WS (GPIO15)
| GND      |  GND
| MUTE     |  -
| VCC      |  VIN 5V


## Additional Comments

I recommend this sketch as a starting point for all I2S input. Just leave the masterclock out, because this is not needed for most input devices.
If it is working with 32 bit you can try to change it to 16bits which most of the time works as well:

- CsvOutput<int16_t> csvStream(Serial);
- cfg.bits_per_sample = 16;

see streams-12s-serial_16_bit