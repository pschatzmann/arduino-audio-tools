# Arduino Audio Tools

Some basic __header-only C++ classes__ that can be used for __Audio Processing__ provided as __Arduino Library__:

- a simple I2S class (to read and write to the internal I2S) 
- a simple ADC class (to read analog data with the help of I2S) 
- a simple PWM class (to write audio data with the help of PWM) 
- Additional Stream implementations: MemoryStream, URLStream, I2SStream, A2DPStream, PrintStream etc
- Support for different [Encoders](https://pschatzmann.github.io/arduino-audio-tools/html/classaudio__tools_1_1_audio_encoder.html) and [Decoders](https://pschatzmann.github.io/arduino-audio-tools/html/classaudio__tools_1_1_audio_decoder.html) for AAC, MP3 and WAV 
- Converters
- Musical Notes (with frequencies of notes)
- SineWaveGenerator (to generate a sine tone) and [Mozzi](https://sensorium.github.io/Mozzi/) for more complex scenario 
- Different buffer implementations 
- TimerAlarmRepeating (e.g. for sampling audio data using exact times) [ESP32 only]
- AudioOutputWithCallback class to provide callback integration e.g. with ESP8266Audio
- Desktop Integration: Building of Arduino Audio Sketches to be run on [Linux, Windows and OS/X](https://github.com/pschatzmann/arduino-audio-tools/wiki/Running-an-Audio-Sketch-on-the-Desktop)

This functionality provides the glue which makes different audio processing components and libraries work together.
We also provide plenty of examples that demonstrate how to implement the different scenarios. The __design philosophy__ is based on the Arduino conventions: we use the ```begin()``` and ```end()``` methods to start and stop the processing and we propagate the __use of Streams__.  We all know the [Arduino Streams](https://pschatzmann.github.io/arduino-audio-tools/html/class_stream.html): We usually use them to write out print messages and sometimes we use them to read the output from Serial devices. The same thing applies to “Audio Streams”: You can read audio data from “Audio Sources” and you write them to “Audio Sinks”.

As “Audio Sources” we will have e.g.:

- Analog Microphones – [AnalogAudioStream](https://pschatzmann.github.io/arduino-audio-tools/html/classaudio__tools_1_1_analog_audio_stream.html)
- Digital Microphones – [I2SStream](https://pschatzmann.github.io/arduino-audio-tools/html/classaudio__tools_1_1_i2_s_stream.html)
- Files on the Internet – [URLStream](https://pschatzmann.github.io/arduino-audio-tools/html/classaudio__tools_1_1_url_stream.html)
- Generated Sound – [GeneratedSoundStream](https://pschatzmann.github.io/arduino-audio-tools/html/classaudio__tools_1_1_generated_sound_stream.html)
- Mobile Phone A2DP Bluetooth – [A2DPStream](https://pschatzmann.github.io/arduino-audio-tools/html/classaudio__tools_1_1_a2_d_p_stream.html)
- Binary Data in Flash Memory – [MemoryStream](https://pschatzmann.github.io/arduino-audio-tools/html/classaudio__tools_1_1_memory_stream.html)
- Desktop Integration - [PortAudioStream](https://pschatzmann.github.io/arduino-audio-tools/html/classaudio__tools_1_1_port_audio_stream.html)
- Any other Arduino Classes implementing Streams: SD, Ethernet etc

As “Audio Sinks” we will have e.g:

- external DAC – [I2SStream](https://pschatzmann.github.io/arduino-audio-tools/html/classaudio__tools_1_1_i2_s_stream.html)
- with an Amplifier – [AnalogAudioStream](https://pschatzmann.github.io/arduino-audio-tools/html/classaudio__tools_1_1_analog_audio_stream.html)
- with a Piezo Electric Element – [PWMAudioStream](https://pschatzmann.github.io/arduino-audio-tools/html/classaudio__tools_1_1_p_w_m_audio_stream_base.html)
- Bluetooth Speakers – [A2DPStream](https://pschatzmann.github.io/arduino-audio-tools/html/classaudio__tools_1_1_a2_d_p_stream.html)
- Serial to display the data as CSV – [CsvStream](https://pschatzmann.github.io/arduino-audio-tools/html/classaudio__tools_1_1_csv_stream.html)
- Encoding and Decoding of Audio [EncodedAudioStream](https://pschatzmann.github.io/arduino-audio-tools/html/classaudio__tools_1_1_encoded_audio_stream.html)
- Desktop Integration - [PortAudioStream](https://pschatzmann.github.io/arduino-audio-tools/html/classaudio__tools_1_1_port_audio_stream.html)
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
StreamCopy copier(i2s, music); // copies sound into i2s

void setup(){
    Serial.begin(115200);

    I2SConfig config = i2s.defaultConfig(TX_MODE);
    config.sample_rate = sample_rate;
    config.channels = channels;
    config.bits_per_sample = 16;
    i2s.begin(config);
}

void loop(){
    if (!copier.copy()){
      i2s.end();
      stop();
    }
}

```

### Examples

Further examples can be found in the [wiki](https://github.com/pschatzmann/arduino-audio-tools/wiki/Examples)!


### Logging

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

### Optional Libraries

Dependent on the example you might need to install some of the following libraries:

- [ESP32-A2DP Library](https://github.com/pschatzmann/ESP32-A2DP) to support A2DP Bluetooth Audio
- [arduino-libhelix](https://github.com/pschatzmann/arduino-libhelix) A __MP3 and AAC Decoder__ from Realnetworks
- [arduino-fdk-aac](https://github.com/pschatzmann/arduino-fdk-aac) A __AAC Encoder and Decoder__ from the Frauenhofer Institute
- [arduino-libmad](https://github.com/pschatzmann/arduino-libmad) A open source __MP3 Decoder__ from Underbit
- [arduino-liblame](https://github.com/pschatzmann/arduino-liblame) A open source __MP3 Encoder__ 
- [SD Library](https://www.arduino.cc/en/reference/SD) to read and write files.
- [Mozzi](https://github.com/pschatzmann/Mozzi) A sound synthesis library for Arduino
- [SAM](https://github.com/pschatzmann/arduino-SAM) A Text to Speach Engine
- [TTS](https://github.com/pschatzmann/TTS) A Text to Speach Engine
- [flite](https://github.com/pschatzmann/arduino-flite) A Text to Speach Engine
- [ESP8266Audio](https://github.com/earlephilhower/ESP8266Audio) to play different audio Formats


### Documentation

- Here is the generated [Class Documentation](https://pschatzmann.github.io/arduino-audio-tools/html/annotated.html). 
- Please also check out the [Information in the Wiki](https://github.com/pschatzmann/arduino-audio-tools/wiki)
- You also might find further information in [one of my Blogs](https://www.pschatzmann.ch/home/category/machine-sound/)


### Installation

You can download the library as zip and call include Library -> zip library. Or you can git clone this project into the Arduino libraries folder e.g. with

```
cd  ~/Documents/Arduino/libraries
git clone pschatzmann/arduino-audio-tools.git

```
