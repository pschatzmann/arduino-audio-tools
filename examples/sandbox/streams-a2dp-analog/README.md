# Receive Sound Data from Bluetooth A2DP

We receive some music via Bluetooth e.g. from your mobile phone and push it out on I2S: Something that the A2DP library does out of the box.
However we just test this function here because the implementation runs via some additional buffers. The critical setting here is to make sure that the buffer for StreamCopy is big enough: we use 4100 bytes.
 
### Internal DAC:

To test the output I am using piezo electric elements

![DAC](https://pschatzmann.github.io/arduino-audio-tools/resources/piezo.jpeg)

It should also be possible to connect a headphone to the output pins.


| PIEZO Left  |  ESP32       
| ------------| --------------
| +           |  GPIO25  
| -           |  GND        

| PIEZO2 Rigt |  ESP32       
| ------------| --------------
| +           |  GPIO26  
| -           |  GND        

