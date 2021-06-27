# Arduino Audio Tools

Some basic __header-only C++ classes__ that can be used for __Audio Processing__ provided as __Arduino Library__:

- a simple I2S class (to read and write to the internal I2S) 
- a simple ADC class (to read analog data with the help of I2S) 
- a simple PWM class (to write audio data with the help of PWM) 
- Additional Stream implementations: MemoryStream, URLStream, I2SStream, A2DPStream, PrintStream, 
- Converters
- Musical Notes (with frequencies of notes)
- SineWaveGenerator (to generate a sine tone) and [Mozzi](https://sensorium.github.io/Mozzi/) for more complex scenario 
- NBuffer (Multi buffer for writing and reading of (audio) data)
- TimerAlarmRepeating (e.g. for sampling audio data using exact times) [ESP32 only]
- A Wav Encoder and Decoder
- AudioOutputWithCallback class to provide callback integration e.g. with ESP8266Audio

This functionality provides the glue which makes different audio processing components and libraries work together.
We also provide plenty of examples that demonstrate how to implement the different scenarios. The __design philosophy__ is based on the Arduino conventions: we use the ```begin()``` and ```end()``` methods to start and stop the processing and we propagate the __use of Streams__.  We all know the [Arduino Streams](https://pschatzmann.github.io/arduino-audio-tools/html/class_stream.html): We usually use them to write out print messages and sometimes we use them to read the output from Serial devices. The same thing applies to “Audio Streams”: You can read audio data from “Audio Sources” and you write them to “Audio Sinks”.

As “Audio Sources” we will have e.g.:

- Analog Microphones – [AnalogAudioStream](https://pschatzmann.github.io/arduino-audio-tools/html/classaudio__tools_1_1_analog_audio_stream.html)
- Digital Microphones – [I2SStream](https://pschatzmann.github.io/arduino-audio-tools/html/classaudio__tools_1_1_i2_s_stream.html)
- Files on the Internet – [URLStream](https://pschatzmann.github.io/arduino-audio-tools/html/classaudio__tools_1_1_url_stream.html)
- Generated Sound – [GeneratedSoundStream](https://pschatzmann.github.io/arduino-audio-tools/html/classaudio__tools_1_1_generated_sound_stream.html)
- Mobile Phone A2DP Bluetooth – [A2DPStream](https://pschatzmann.github.io/arduino-audio-tools/html/classaudio__tools_1_1_a2_d_p_stream.html)
- Binary Data in Flash Memory – [MemoryStream](https://pschatzmann.github.io/arduino-audio-tools/html/classaudio__tools_1_1_memory_stream.html)
- Any other Arduino Classes implementing Streams: SD, Ethernet etc

As “Audio Sinks” we will have e.g:

- external DAC – [I2SStream](https://pschatzmann.github.io/arduino-audio-tools/html/classaudio__tools_1_1_i2_s_stream.html)
- an Amplifier – [AnalogAudioStream](https://pschatzmann.github.io/arduino-audio-tools/html/classaudio__tools_1_1_analog_audio_stream.html)
- Earphones – [PWMAudioStream](https://pschatzmann.github.io/arduino-audio-tools/html/classaudio__tools_1_1_pwm_audio_stream.html)
- Bluetooth Speakers – [A2DPStream](https://pschatzmann.github.io/arduino-audio-tools/html/classaudio__tools_1_1_a2_d_p_stream.html)
- Serial to display the data as CSV – [CsvStream](https://pschatzmann.github.io/arduino-audio-tools/html/classaudio__tools_1_1_csv_stream.html)
- Any other Arduino Classes implementing Streams: SD, Ethernet etc

Here is an simple example which streams a file from the Flash Memory and writes it to I2S: 

```
#include "AudioTools.h"
#include "StarWars30.h"

using namespace audio_tools;  

uint8_t channels = 2;
uint16_t sample_rate = 22050;

MemoryStream music(StarWars30_raw, StarWars30_raw_len);
I2SStream i2s;  // Output to I2S
StreamCopyT<int16_t> copier(i2s, music); // copies sound into i2s

void setup(){
    Serial.begin(115200);

    I2SConfig config = i2s.defaultConfig(TX_MODE);
    config.sample_rate = sample_rate;
    config.channels = channels;
    config.bits_per_sample = 16;
    i2s.begin(config);
}

void loop(){
    if (!copier.copy2()){
      i2s.end();
      stop();
    }
}

```
A complete list of the supported Audio Stream classes and scenarios can be found in the [Scenarios Document](Scenarios.md)

### Sound Output

- __I2SStream__: The best quality can be achieved with the help of I2S and an external DAC.  I2S is supporting 2 channels only. 
- __AnalogAudioStream__: Some processors are providing an analog output, this is usually an easy and good approach: The number of pins (and herewith output channels) however is usually very limited.
- __PWMAudioStream__: The last possibility is to simulate an analog output with the help of PWM by using a frequency which is beyond the audible range of 20 KHz. This method is supported by all processor and usually supports a bigger number of output pins. In terms of audio quality this is usually the worst option.


## Examples

The examples follow the following naming convention: "scenario type"-"source"-"destination". For the scenario types we might have __base__ (using basic api functionality), __stream__ for examples using Streams and __test__ for the test cases. 

For the __source__ we currently have __adc__ for analog input devices like analog microphones, __i2s__ for digital input devices (e.g. digital microphones), __file__ for SD files and __a2dp__ for input from Bluetooth A2DP (e.g. from a Mobile Phone).

For the __destination__ we use __dac__ for analog output (e.g. to an amplifier), __i2s__ for digital output devices (e.g. an external DAC), __file__ for SD files and __a2dp__ for output to Bluetooth A2DP (e.g. a Bluetooth Speaker).


Here is the list of examples:

#### Stream API

Here are a couple of simple test sketches to demo different output destinations:

- [streams-generator-serial](examples/streams-generator-serial) Displaying generated sound on the Serial Plotter
- [streams-generator-i2s](examples/streams-generator-i2s) Output of generated sound on external DAC via I2S
- [streams-generator-dac](examples/streams-generator-dac) Output of generated sound on ESP32 internal DAC via I2S
- [streams-generator-a2dp](examples/streams-generator-a2dp) Output of generated sound on Bluetooth Speaker using A2DP
- [streams-adc-serial](examples/streams-adc-serial) Displaying input from analog microphone on the Serial Plotter
- [streams-memory_wav-serial](examples/streams-memory_wav-serial) Decoding of WAV from Flash memory and display on the Serial Plotter

And some more useful examples:

- [streams-memory_raw-i2s](examples/streams-memory_raw-i2s) - Play music form Flash Memory via I2S to External DAC
- [streams-url_raw-serial](examples/streams-url_raw-serial) Displaying a music file from the internet on the Serial Plotter
- [streams-url_raw-I2S.ino](examples/streams-url_raw-i2s) Streaming a File from the Internet to on external DAC via I2S
- [streams-mozzi-a2dp](examples/streams-mozzi-a2dp) Use Mozzi to generate Sound to be sent to a Bluetooth Speaker
- [streams-url_wav-i2s](examples/streams-url_wav-i2s) Text to Speach example using Rhasspy

... these are just a few examples, but you can combine any Input Stream with any Output Stream as you like...

#### Basic API

- [base-adc-serial](basic-api/base-adc-serial) - Sample analog sound and write it to Serial
- [base-adc-a2dp](basic-api/base-adc-a2dp) - Sample analog sound and write it to a A2DP Bluetooth source 
- [base-file_raw-serial](basic-api/base-file_raw-serial) - Read Raw File from SD card to and write it to Serial
- [base-file_raw-a2dp](basic-api/base-file_raw-a2dp) - Read Raw File from SD card write it A2DP Bluetooth
- [base-file_mp3-a2dp](basic-api/base-file_mp3-a2dp) - Stream MP3 File from SD card to A2DP Bluetooth using the ESP8266Audio library
- [base-i2s-serial](basic-api/base-i2s-serial) - Sample digital sound and write it to Serial
- [base-i2s-a2dp](basic-api/base-i2s-a2dp) - Sample analog sound and write it to a A2DP Bluetooth source 

#### Listening to the Result with a Webbrowser

I am also providing a simple webserver which can render the audio data as wav result. 
Here are some examples:

- [streams-generator-webserver_wav](examples/streams-generator-webserver_wav) A Webserver which renders some generated sound
- [streams-sam-webserver_wav](examples/streams-sam-webserver_wav) A Webserver which renders the result from the SAM TTS engine
- [streams-tts-webserver_wav](examples/streams-tts-webserver_wav) A Webserver which renders the result from the Arduino TTS engine
- [streams-flite-webserver_wav](examples/streams-flite-webserver_wav) A Webserver which renders the result from the Flite TTS engine

#### Logging

The application uses a built in logger (see AudioLogger.h and AudioConfig.h). You can  e.g. deactivate the logging by changing USE_AUDIO_LOGGING to false in the AudioConfig.h: 

```
#define USE_AUDIO_LOGGING false
#define LOG_LEVEL AudioLogger::Warning
#define LOG_STREAM Serial
```

Per default we use the log level warning and the logging output is going to Serial. You can also change this in your sketch by calling AudioLogger begin with the output stream and the log level e.g:

```
AudioLogger::instance().begin(Serial, AudioLogger::Debug);
```


## Optional Libraries

Dependent on the example you might need to install some of the following libraries:

- [ESP32-A2DP Library](https://github.com/pschatzmann/ESP32-A2DP) to support A2DP Bluetooth Audio
- [ESP8266Audio](https://github.com/earlephilhower/ESP8266Audio) to play different audio Formats
- [SD Library](https://www.arduino.cc/en/reference/SD) to read and write files.
- [arduino-fdk-aac](https://github.com/pschatzmann/arduino-fdk-aac) to encode or decode AAC 
- [Mozzi](https://github.com/pschatzmann/Mozzi) A sound synthesis library for Arduino
- [SAM](https://github.com/pschatzmann/arduino-SAM) A Text to Speach Engine
- [TTS](https://github.com/pschatzmann/TTS) A Text to Speach Engine
- [flite](https://github.com/pschatzmann/arduino-flite) A Text to Speach Engine

## Installation

You can download the library as zip and call include Library -> zip library. Or you can git clone this project into the Arduino libraries folder e.g. with

```
cd  ~/Documents/Arduino/libraries
git clone pschatzmann/arduino-audio-tools.git

```

## Documentation

Here is the generated [Class documentation](https://pschatzmann.github.io/arduino-audio-tools/html/annotated.html). 

You also might find further information in [one of my blogs](https://www.pschatzmann.ch/home/category/machine-sound/)

## Project Status

This is currently work in progress:

| Functionality          | Status  |
|------------------------|---------|
| Analog input - ADC     | tested  |
| I2S                    | tested  |
| Files (RAW, MP3...)    | tested  |
| Streams                | tested  |
| WAV encoding/deconding | tested  |          
| AAC encoding/deconding | open    |          
| int24_t                | tested  |           

