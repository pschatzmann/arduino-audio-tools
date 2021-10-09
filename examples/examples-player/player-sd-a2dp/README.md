# A Simple SdFat Audio Player

It is pretty simple to build a simple audio player with the help of the Stream API. A SD file is a sublass of an Arduino Stream, so all you need to do is to copy from the file to the desired output strem. Finally you need to add some logic which handles the end of file to automatically process the next file and maybe a status flag to halt and contine the processing. In addition it adds only a little bit of additional complexity to add volume control and meta data support.

In order to simplify things, I decided to provide this functionality as well and to prove the point: The AudioPlayer class took only 120 lines of code to implement!

The AudioPlayer supports 

- __multiple processor architectures__
- __multiple audio data sources__ (SD, URL, callbacks)
- __different Output__ Scenarios (I2S, PWM, A2DP etc). Just pass the desired output stream object to the constructor.
- __different Decoders__ for MP3, AAC, WAV. Just pass the desired decoder object to the constructor.
- __Volume Control__ (by calling player.setVolume())
- __Stopping and Resuming__ the processing (by calling player.stop() and player.play())
- You can __move to the next file__ by calling player.next();
- support for __Metadata__

The example demonstrates how to implement an __MP3 Player__: which provides the data from a SD drive and sends the audio via A2DP to an A2DP Sink (e.g. Bluetooth Speaker): The __AudioSourceSdFat class__ builds on the [SdFat Library](https://github.com/greiman/SdFat) from Bill Greiman which provides FAT16/FAT32 and exFAT support. 

## Dependencies

- https://github.com/pschatzmann/arduino-audio-tools
- https://github.com/pschatzmann/arduino-libhelix
- https://github.com/pschatzmann/ESP32-A2DP
- https://github.com/greiman/SdFat
