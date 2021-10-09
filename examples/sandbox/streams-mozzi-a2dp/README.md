# Using Mozzi on Bluetooth

I am providing a simple integration for [Mozzi](https://sensorium.github.io/Mozzi/).  

A standard Sketch will need to combine the Audio-Tools with the Mozzi Logic:

```
#include "AudioTools.h"
#include "AudioA2DP.h"
#include "AudioMozzi.h"
// Mozzi includes
// ....


using namespace audio_tools;  

// Audio Tools
MozziGenerator mozzi(CONTROL_RATE);                       // subclass of SoundGenerator 
GeneratedSoundStream<int16_t> in(mozzi, 2);               // Stream with 2 channels generated with mozzi
A2DPStream out = A2DPStream::instance() ;                 // A2DP input - A2DPStream is a singleton!
StreamCopy copier(out, in);                               // copy in to out 

```

As you can see above you can add the __MozziGenerator__ like any other Generator to the __GeneratedSoundStream__ class.
In the setup you need to combine the Audio-Tools setup and then call the Mozzi specific setup functions. Replace the __startMozzi()__ with our __GeneratedSoundStream.begin()__:

```
void setup(){
  Serial.begin(115200);

  if (mozzi.config().sample_rate!=44100){
    Serial.println("Please set the AUDIO_RATE in the mozzi_config.h to 44100");
    stop();
  }

  // We send the generated sound via A2DP - so we conect to the MyMusic Bluetooth Speaker
  out.begin(TX_MODE, "MyMusic");
  Serial.println("A2DP is connected now...");

  // Add your Mozzi setup here
  // ...
  in.begin();
}
```

Like in any other Mozzi Sketch you need to define the __updateControl()__

```
// Mozzi updateControl
void updateControl(){
}
```

And like in any other Mozzi Sketch you need to define the __updateAudio()__

```
// Mozzi updateAudio
AudioOutput_t updateAudio(){
}
```

In the loop you need to replace the __audioHook()__ with your copy logic:

```
// Arduino loop  
void loop() {
  if (out)
    copier.copy();
}
```


### Mozzi Configuration 

A2DP requires an audio rate of 44100. Therefore you need to make sure that you have the following settings in your ```mozzi_config.h```:

```
#define AUDIO_RATE 44100
#define MICROS_PER_AUDIO_TICK 1000000 / AUDIO_RATE 
```


That's all to output the generated sound to a Bluetooth Speaker...
