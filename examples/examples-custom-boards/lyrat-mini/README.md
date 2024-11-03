The LyraT Mini is a bit tricky to use because it has a ES8311 which is handling the output and a  
ES7243 which is handling the microphone input: both need to be on separate I2S ports.

You should be able to use the examples that can be found in the [audiokit directory](https://github.com/pschatzmann/arduino-audio-tools/tree/main/examples/examples-audiokit): just replace the
driver with LyratMini!

For the examples install:

- [Arduino AudioTools](https://github.com/pschatzmann/arduino-audio-tools)
- [Arduino Audio Driver](https://github.com/pschatzmann/arduino-audio-driver)