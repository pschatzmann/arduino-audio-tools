# Simple TTS to A2DP

This is just an adaptation of the [streams-simple_tts-i2s example](https://github.com/pschatzmann/arduino-audio-tools/tree/main/examples/examples-tts/streams-simple_tts-i2s) where the output stream has been replaced with a A2DPStream.

So the output goes to a Bluetooth Speaker! 

More examples can be found at https://github.com/pschatzmann/arduino-simple-tts/tree/main/examples

Because we need a lot of progmem, do not forget to set the partition scheme to Huge APP!

### Dependencies

- https://github.com/pschatzmann/ESP32-A2DP.git
- https://github.com/pschatzmann/arduino-simple-tts


A word of warning: The A2DP output has it's challenges, so I do not recommend this as your first sketch.