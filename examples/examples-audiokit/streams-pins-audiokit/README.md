# AudioActions

Originally I was of the opinion that the program logic for __reading of the GPIO pins__ and the processing of the related actions should belong into a specific __Arduino sketch__ and should not be part of an Audio Framework. 

The [Audio Kit](https://docs.ai-thinker.com/en/esp32-audio-kit) made me revise this because I think that an Arduino Sketch should be __as concise as possible__. For this I have created the AudioActions class which has been integrated into the AudioKit.

Since the Audio Kit does not have any display, I think to use TTS is a good choice. 
You just need to define you __handler methods__ (button1, button2...) and assign them to the __GPIO pins__. 


### Dependencies

You need to install the following libraries:

- [Arduino Audio Tools](https://github.com/pschatzmann/arduino-audio-tools)
- [AudioKit](https://github.com/pschatzmann/arduino-audiokit)
- [FLITE](https://github.com/pschatzmann/arduino-flite)

