# Arduino Audio Tools

Some basic __header-only C++ classes__ that can be used for __Audio Processing__ provided as __Arduino and cmake C++ Library__:

- We provide different ["Audio Sources" and "Audio Sinks"](https://github.com/pschatzmann/arduino-audio-tools/wiki/Audio-Sources-and-Sinks)
- Support for different [Encoders](https://pschatzmann.github.io/arduino-audio-tools/classaudio__tools_1_1_audio_encoder.html) and [Decoders](https://pschatzmann.github.io/arduino-audio-tools/classaudio__tools_1_1_audio_decoder.html) for MP3, AAC, WAV, FLAC, etc for [EncodedAudioStream](https://pschatzmann.github.io/arduino-audio-tools/classaudio__tools_1_1_encoded_audio_stream.html)
- Integrates with different [DSP libraries](https://github.com/pschatzmann/arduino-audio-tools/wiki/DSP-Libraries)
- Helps to [communicate](https://github.com/pschatzmann/arduino-audio-tools/wiki/Communication) audio over the wire or wirelessly
- Different [Sound Generators](https://pschatzmann.github.io/arduino-audio-tools/group__generator.html) (e.g. to generate a sine tone) for [GeneratedSoundStream](https://pschatzmann.github.io/arduino-audio-tools/classaudio__tools_1_1_generated_sound_stream.html)
- Support for [Sound Effects](https://pschatzmann.github.io/arduino-audio-tools/classaudio__tools_1_1_audio_effect_stream.html) with different [Effect Implementations](https://pschatzmann.github.io/arduino-audio-tools/classaudio__tools_1_1_audio_effect.html) (e.g. Boost, Distortion, Echo, Reverb...) for [AudioEffectStream](https://pschatzmann.github.io/arduino-audio-tools/classaudio__tools_1_1_audio_effect_stream_t.html)
- Provides a [3 Band Equalizer](https://pschatzmann.github.io/arduino-audio-tools/classaudio__tools_1_1_equalizer3_bands.html)
- Different [Converters](https://pschatzmann.github.io/arduino-audio-tools/classaudio__tools_1_1_base_converter.html) for [ConverterStream](https://pschatzmann.github.io/arduino-audio-tools/classaudio__tools_1_1_converter_stream.html)
- Different [Filters](https://pschatzmann.github.io/arduino-audio-tools/classaudio__tools_1_1_filter.html) for [FilteredStream](https://pschatzmann.github.io/arduino-audio-tools/classaudio__tools_1_1_filtered_stream.html)
- [Musical Notes](https://pschatzmann.github.io/arduino-audio-tools/classaudio__tools_1_1_musical_notes.html) (with frequencies of notes)
- Different [Buffer Implementations](https://pschatzmann.github.io/arduino-audio-tools/classaudio__tools_1_1_base_buffer.html) for [QueueStream](https://pschatzmann.github.io/arduino-audio-tools/classaudio__tools_1_1_queue_stream.html) 
- A [Repeating Timer](https://pschatzmann.github.io/arduino-audio-tools/classaudio__tools_1_1_timer_alarm_repeating.html) (e.g. for sampling audio data using exact times) for [TimerCallbackAudioStream](https://pschatzmann.github.io/arduino-audio-tools/classaudio__tools_1_1_timer_callback_audio_stream.html)
- Desktop Integration: Building of Arduino Audio Sketches to be run on [Linux, Windows and OS/X](https://github.com/pschatzmann/arduino-audio-tools/wiki/Running-an-Audio-Sketch-on-the-Desktop)

This functionality provides the glue which makes different audio processing components and [libraries](https://github.com/pschatzmann/arduino-audio-tools/wiki/Optional-Libraries) work together.

We also provide [plenty of examples](https://github.com/pschatzmann/arduino-audio-tools/wiki/Examples) that demonstrate how to implement the different scenarios. The __design philosophy__ is based on the Arduino conventions: we use the ```begin()``` and ```end()``` methods to start and stop the processing and we propagate the __use of Streams__.  

We all know the Arduino [Print](https://www.arduino.cc/reference/en/language/functions/communication/print/) and [Stream](https://www.arduino.cc/reference/en/language/functions/communication/stream) classes: We usually use them to write out print messages and sometimes we use them to read the output from Serial, Files, Ethernet, etc. The same thing applies to “Audio Streams”: You can read audio data from [“Audio Sources” and you write them to “Audio Sinks”](https://github.com/pschatzmann/arduino-audio-tools/wiki/Audio-Sources-and-Sinks).


### Example

Here is a simple example which streams a file from the Flash Memory and writes it to I2S: 

```C++
#include "AudioTools.h"
#include "StarWars30.h"

uint8_t channels = 2;
uint16_t sample_rate = 22050;
uint8_t bits_per_sample = 16;

MemoryStream music(StarWars30_raw, StarWars30_raw_len);
I2SStream i2s;  // Output to I2S
StreamCopy copier(i2s, music); // copies sound into i2s

void setup(){
    Serial.begin(115200);

    auto config = i2s.defaultConfig(TX_MODE);
    config.sample_rate = sample_rate;
    config.channels = channels;
    config.bits_per_sample = bits_per_sample;
    i2s.begin(config);

    music.begin();
}

void loop(){
    copier.copy();
}

```
Each stream has it's own [configuration object](https://pschatzmann.github.io/arduino-audio-tools/structaudio__tools_1_1_audio_info.html) that should be passed to the begin method. The defaultConfig() method is providing a default proposal which will usually "just work". Please consult 
the [class documentation](https://pschatzmann.github.io/arduino-audio-tools/modules.html) for the available configuration parameters. You can also __easily adapt__ any provided examples: If you e.g. replace the I2SStream with the AnalogAudioStream class, you will get analog instead of digital output.

I suggest you continue to read the more [detailed introduction](https://github.com/pschatzmann/arduino-audio-tools/wiki/Introduction).

Further examples can be found in the [Wiki](https://github.com/pschatzmann/arduino-audio-tools/wiki/Examples). 

Dependent on the example you might need to install some [additional libaries](https://github.com/pschatzmann/arduino-audio-tools/wiki/Optional-Libraries)

### AudioPlayer

The library also provides a versatile [AudioPlayer](https://pschatzmann.github.io/arduino-audio-tools/classaudio__tools_1_1_audio_player.html). Further information can be found in the [Wiki](https://github.com/pschatzmann/arduino-audio-tools/wiki/The-Audio-Player-Class)


### Logging

The application uses a built in logger: By default we use the log level warning and the logging output is going to Serial. You can change this in your sketch by calling e.g:

```C++
AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Debug);
```
You can log to any object that is a subclass of Print and valid log level values are: Debug, Info, Warning, Error.


You can also deactivate the logging by changing USE_AUDIO_LOGGING to false in the AudioConfig.h to decrease the memory usage: 

```C++
#define USE_AUDIO_LOGGING false
```

## Show and Tell

Get some inspiration [from projects that were using this library](https://github.com/pschatzmann/arduino-audio-tools/discussions/categories/show-and-tell) or share your projects with the community.


## Documentation

Please use this before you raise any issue or start a discussion!

- Read the [Tutorial & Documentation in the Wiki](https://github.com/pschatzmann/arduino-audio-tools/wiki)
- Have a look at the [Examples](https://github.com/pschatzmann/arduino-audio-tools/wiki/Examples)
- Check the [Class Documentation by Topic](https://pschatzmann.github.io/arduino-audio-tools/topics.html). 
- Find your class in [All Classes Alphabetically](https://pschatzmann.github.io/arduino-audio-tools/namespaceaudio__tools.html)
- You also might find further information in [one of my Blogs](https://www.pschatzmann.ch/home/category/machine-sound/)

## Support

I spent a lot of time to provide a comprehensive and complete documentation.
So please read the documentation first and check the issues and discussions before posting any new ones on Github.

Open issues only for bugs and if it is not a bug, use a discussion: Provide enough information about 
- the selected scenario/sketch 
- what exactly you are trying to do
- your hardware
- your software version (from the Boards Manager) and library versions
- what exactly your problem is

to enable others to understand and reproduce your issue.

Finally, __don't__ send me any e-mails or post questions on my personal website! 

Please note that discussions and issues which have already been answered before or where the answer can be found in the documentation may get deleted w/o reply.

## Installation in Arduino

You can download the library as zip and call include Library -> zip library. Or you can git clone this project into the Arduino libraries folder e.g. with

```
cd  ~/Documents/Arduino/libraries
git clone https://github.com/pschatzmann/arduino-audio-tools.git
```

I recommend to use git because you can easily update to the latest version just by executing the ```git pull``` command in the project folder.
If you want to use the library on other patforms, you can find [further information in the Wiki](https://github.com/pschatzmann/arduino-audio-tools/wiki). 


## Sponsor Me

This software is totally free, but you can make me happy by rewarding me with a treat

- [Buy me a coffee](https://www.buymeacoffee.com/philschatzh)
- [Paypal me](https://paypal.me/pschatzmann?country.x=CH&locale.x=en_US)
