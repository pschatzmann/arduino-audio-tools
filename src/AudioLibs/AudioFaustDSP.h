#pragma once 

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

/**
 * @brief minimal dsp base class needed by Faust
 * 
 */
class dsp {
    public:
	    virtual void init(int sample_rate) = 0;
	    virtual void compute(int count, FAUSTFLOAT** inputs, FAUSTFLOAT** outputs) = 0;
        virtual void instanceClear() = 0;
    	virtual int getNumInputs() = 0;
	    virtual int getNumOutputs() = 0;
	
};
/**
 * @brief minimial implementtion of Meta which just ignores the data
 * 
 */
class Meta {
    public:
        void declare(const char*, const char*){}
};

typedef void Soundfile;

/**
 * @brief Minimum implementation of UI
 * 
 */
class UI {
  public:
    // -- widget's layouts
    virtual void openTabBox(const char* label) {}
    virtual void openHorizontalBox(const char* label) {}
    virtual void openVerticalBox(const char* label) {}
    virtual void closeBox() {}

    // -- active widgets
    virtual void addButton(const char* label, FAUSTFLOAT* zone) {}
    virtual void addCheckButton(const char* label, FAUSTFLOAT* zone) {}
    virtual void addVerticalSlider(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step) {}
    virtual void addHorizontalSlider(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step) {}
    virtual void addNumEntry(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step) {}

    // -- passive widgets
    virtual void addHorizontalBargraph(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max) {}
    virtual void addVerticalBargraph(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max) {}

    // -- soundfiles
    virtual void addSoundfile(const char* label, const char* filename, Soundfile** sf_zone) {}

    // -- metadata declarations
    virtual void declare(FAUSTFLOAT* zone, const char* key, const char* val) {}
};
