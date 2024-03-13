#pragma once
#include "StkAllh.h"

/**
 * @brief Demo how you can compose your own instrument
 * @author Phil Schatzmann
 */
class MyFirstInstrument : public Instrmnt  {
  public:
    MyFirstInstrument() {
      adsr.setAllTimes( 0.005, 0.01, 0.8, 0.010 );
      echo.setDelay(1024);
    }

     //! Start a note with the given frequency and amplitude.
    void noteOn( StkFloat frequency, StkFloat amplitude ) {
       wave.setFrequency(frequency);
       adsr.keyOn();
    }

     //! Stop a note with the given amplitude (speed of decay).
    void noteOff( StkFloat amplitude ){
       adsr.keyOff();
    }

    float tick() {
       return echo.tick(wave.tick()) * adsr.tick();
    }

  protected:
    stk::SineWave wave;
    stk::ADSR adsr;
    stk::Echo echo {1024};
};
