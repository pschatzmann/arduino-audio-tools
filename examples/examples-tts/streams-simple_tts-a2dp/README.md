# Simple TTS

I am providing a simple sketch which generates sound data with my Simple TTS text to speach engine that 
uses a configurable library of prerecorded words.

In this demo we provide the output to a Bluetooth Speaker 

More examples can be found at https://github.com/pschatzmann/arduino-simple-tts/tree/main/examples

Because we need a lot of progmem, do not forget to set the partition scheme to Huge APP!

### Dependencies

- https://github.com/pschatzmann/ESP32-A2DP.git
- https://github.com/pschatzmann/arduino-simple-tts


A word of warning: The A2DP output has it's challenges, so I do not recommend this as your first sketch.