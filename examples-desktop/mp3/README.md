# Playing Sound on your Desktop

We provide some generic output which will also work on Linux, Windows and OS/X
The cmake is downloading all dependencies and builds an executable from the sketch.

You just need to provide an Arduino Sketch as cpp file. To build the example execute:

```
mkdir build
cd build
cmake ..
make
```

Unfortunatly the example does not generate any sound because of __Output underflowes__: The mp3 decoder can not provide the audio data fast enough...