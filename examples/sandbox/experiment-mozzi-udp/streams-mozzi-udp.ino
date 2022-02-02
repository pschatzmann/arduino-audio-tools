/**
 * @file streams-generator-a2dp.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-stream/streams-generator-a2dp/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
#include "AudioTools.h"
#include "AudioNet.h"
#include "AudioMozzi.h"
#include "WiFi.h"
#include <Oscil.h>
#include <tables/cos2048_int8.h> // table for Oscils to play
#include <mozzi_fixmath.h>
#include <EventDelay.h>
#include <mozzi_rand.h>
#include <mozzi_midi.h>



typedef int16_t sound_t;                                  // sound will be represented as int16_t (with 2 bytes)
uint8_t channels = 2;                                     // The stream will have 2 channels 
MozziGenerator mozzi(CONTROL_RATE);                       // subclass of SoundGenerator 
GeneratedSoundStream<sound_t> in(mozzi);        // Stream generated with mozzi
UDPStream out;                                            // UDP Output 
StreamCopy copier(out, in); // copy in to out

/// Copied from AMsynth.ino
#define CONTROL_RATE 64 // Hz, powers of 2 are most reliable

// audio oscils
Oscil<COS2048_NUM_CELLS, AUDIO_RATE> aCarrier(COS2048_DATA);
Oscil<COS2048_NUM_CELLS, AUDIO_RATE> aModulator(COS2048_DATA);
Oscil<COS2048_NUM_CELLS, AUDIO_RATE> aModDepth(COS2048_DATA);

// for scheduling note changes in updateControl()
EventDelay  kNoteChangeDelay;

// synthesis parameters in fixed point formats
Q8n8 ratio; // unsigned int with 8 integer bits and 8 fractional bits
Q24n8 carrier_freq; // unsigned long with 24 integer bits and 8 fractional bits
Q24n8 mod_freq; // unsigned long with 24 integer bits and 8 fractional bits

// for random notes
Q8n0 octave_start_note = 42;


void setup(){
  Serial.begin(115200);

  // connect to WIFI
  WiFi.begin("network", "password");
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(500); 
  }
  Serial.println("Connected to WIFI");

  if (mozzi.config().sample_rate!=44100){
    Serial.println("Please set the AUDIO_RATE in the mozzi_config.h to 44100");
    stop();
  }

  // We send the sound via UDP (to a broadcast address)
  IPAddress ip(192, 168, 1, 255);
  out.begin(ip, 3333);

  ratio = float_to_Q8n8(3.0f);   // define modulation ratio in float and convert to fixed-point
  kNoteChangeDelay.set(200); // note duration ms, within resolution of CONTROL_RATE
  aModDepth.setFreq(13.f);     // vary mod depth to highlight am effects
  randSeed(); // reseed the random generator for different results each time the sketch runs
  in.begin();
}

void updateControl(){
  static Q16n16 last_note = octave_start_note;

  if(kNoteChangeDelay.ready()){

    // change octave now and then
    if(rand((byte)5)==0){
      last_note = 36+(rand((byte)6)*12);
    }

    // change step up or down a semitone occasionally
    if(rand((byte)13)==0){
      last_note += 1-rand((byte)3);
    }

    // change modulation ratio now and then
    if(rand((byte)5)==0){
      ratio = ((Q8n8) 1+ rand((byte)5)) <<8;
    }

    // sometimes add a fractionto the ratio
    if(rand((byte)5)==0){
      ratio += rand((byte)255);
    }

    // step up or down 3 semitones (or 0)
    last_note += 3 * (1-rand((byte)3));

    // convert midi to frequency
    Q16n16 midi_note = Q8n0_to_Q16n16(last_note);
    carrier_freq = Q16n16_to_Q24n8(Q16n16_mtof(midi_note));

    // calculate modulation frequency to stay in ratio with carrier
    mod_freq = (carrier_freq * ratio)>>8; // (Q24n8   Q8n8) >> 8 = Q24n8

      // set frequencies of the oscillators
    aCarrier.setFreq_Q24n8(carrier_freq);
    aModulator.setFreq_Q24n8(mod_freq);

    // reset the note scheduler
    kNoteChangeDelay.start();
  }
}

AudioOutput_t updateAudio(){
  int32_t mod = (128u+ aModulator.next()) * ((byte)128+ aModDepth.next());
  return MonoOutput::fromNBit(24, mod * aCarrier.next());
}

// Arduino loop  
void loop() {
  if (out)
    copier.copy();
}