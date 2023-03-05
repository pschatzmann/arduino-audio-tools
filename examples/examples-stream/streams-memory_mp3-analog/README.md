# Decoding a MP3 file

In this example we decode a MP3 file into RAW output and send it the analog output pins. 

Currently we provide multiple mp3 decoders
- libhelix 
- tinymp3

Unfortunatly non of those are currently stable in all environments....

### Output Device: Piezo Electric Element

To test the output I am using piezo electric elements

![DAC](https://pschatzmann.github.io/Resources/img/piezo.jpeg)

It should also be possible to connect a headphone to the output pins.


| PIEZO Left  |  ESP32       
| ------------| --------------
| +           |  GPIO25  
| -           |  GND        

| PIEZO2 Rigt |  ESP32       
| ------------| --------------
| +           |  GPIO26  
| -           |  GND        
