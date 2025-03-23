#pragma once
// include to identify future compile errors: if you don't have all dependencies
// installed this will generate a lot of compile errors!
#include "MozziStream.h"
#include "A2DPStream.h"
#include "AudioBoardStream.h"
#include "AudioClientRTSP.h"
#include "AudioEffectsSuite.h"
#include "AudioFFT.h"
#include "AudioSTK.h"
#include "Concurrency.h"
#include "FFTEffects.h"
#include "HLSStream.h"
#include "I2SCodecStream.h"
#include "LEDOutput.h"
#include "MaximilianDSP.h"
#include "MemoryManager.h"
#include "PIDController.h"
#include "R2ROutput.h"
#include "SPDIFOutput.h"
#include "StdioStream.h"
#include "VBANStream.h"
#include "VS1053Stream.h"
//  #include "TfLiteAudioStream.h"  // takes too much time
//  #include "AudioServerEx.h"
//  #include "WM8960Stream.h"      // driver part of AudioBoardStream
//  #include "AudioFaust.h"
//  #include "RTSP.h"              // conflit with AudioClientRTSP
//  #include "AudioESP32ULP.h"     // using obsolete functioinality
//  #include "PureDataStream.h"
//  #include "Jupyter.h"           // only for desktop
//  #include "PortAudioStream.h"   // only for desktop
//  #include "MiniAudioStream.h"   // only for desktop
//  #include "AudioKissFFT.h"      // select on fft implementation
//  #include "AudioCmsisFFT.h"     // select on fft implementation
//  #include "AudioRealFFT.h"      // select on fft implementation
//  #include "AudioESP32FFT.h"     // select on fft implementation
//  #include "AudioEspressifFFT.h" // select on fft implementation
//  #include "FFTDisplay.h"
//  #include "LEDOutputUnoR4.h"    // only for uno r4
//  #include "AudioMP34DT05.h"     // only for nano ble sense
//  #include "AudioESP8266.h"
//  #include "AudioKit.h"          // obsolete
