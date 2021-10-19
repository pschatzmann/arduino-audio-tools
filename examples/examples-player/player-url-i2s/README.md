# A Simple Streaming Audio Player

It was pretty simple to build a simple audio player with the help of the Stream API. 

The AudioPlayer supports 

- __multiple processor architectures__
- __multiple audio data sources__ (SD, URL, callbacks)
- __different Output__ Scenarios (I2S, PWM, A2DP etc). Just pass the desired output stream object to the constructor.
- __different Decoders__ for MP3, AAC, WAV. Just pass the desired decoder object to the constructor.
- __Volume Control__ (by calling player.setVolume())
- __Stopping and Resuming__ the processing (by calling player.stop() and player.play())
- You can __move to the next file__ by calling player.next();
- support for __Metadata__


The example demonstrates how to implement an __MP3 Player__ which provides the data via the Internet (with the help of the AudioSourceURL class) and sends the audio via I2S to an external DAC.

### External DAC:

For my tests I am using the 24-bit PCM5102 PCM5102A Stereo DAC Digital-to-analog Converter PLL Voice Module pHAT

![DAC](https://pschatzmann.github.io/arduino-audio-tools/resources/dac.jpeg)

I am just using the default pins defined by the framework. However I could change them with the help of the config object. The mute pin can be defined in the constructor of the I2SStream - by not defining anything we use the default which is GPIO23

 
| DAC     |  ESP32
| --------| ---------------
| VDD     |  5V
| GND     |  GND
| SD      |  OUT (GPIO22)
| L/R     |  GND
| WS      |  WS (GPIO15)
| SCK     |  BCK (GPIO14)
| FMT     |  GND 
| XSMT    |  +3V 


- DEMP - De-emphasis control for 44.1kHz sampling rate(1): Off (Low) / On (High)
- FLT - Filter select : Normal latency (Low) / Low latency (High)
- SCK - System clock input (probably SCL on your board).
- FMT - Audio format selection : I2S (Low) / Left justified (High)
- XSMT - Soft mute control(1): Soft mute (Low) / soft un-mute (High)


## Dependencies

- https://github.com/pschatzmann/arduino-audio-tools
- https://github.com/pschatzmann/arduino-libhelix
