#pragma once 

#include "AudioBasic/Vector.h"

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 


// forward declarations
class UI;


/**
 * @brief minimal dsp base class needed by Faust
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class dsp {
    public:
	    virtual void init(int sample_rate) = 0;
	    virtual void compute(int count, FAUSTFLOAT** inputs, FAUSTFLOAT** outputs) = 0;
        virtual void instanceClear() = 0;
    	virtual int getNumInputs() = 0;
	    virtual int getNumOutputs() = 0;
        virtual void buildUserInterface(UI* ui_interface) = 0;
};

/**
 * @brief minimial implementtion of Meta which just ignores the data
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class Meta {
    public:
        void declare(const char*, const char*){}
};

typedef void Soundfile;

/**
 * @brief Minimum implementation of UI parameters. We only support the setting and getting of values
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class UI {
  struct Entry {
      const char* label=nullptr;
      FAUSTFLOAT* zone=nullptr;
      bool withLimits;
      FAUSTFLOAT min;
      FAUSTFLOAT max;
  }; 

  public:
    // set and get values 
    virtual FAUSTFLOAT getValue(const char*label) {
        Entry *e = findEntry(label);
        if (e==nullptr){
            LOGE("Label '%s' not found", label);
        }
        return e!=nullptr ? *(e->zone) : 0.0;
    }
    virtual bool setValue(const char*label, FAUSTFLOAT value){
        bool result = false;
        Entry* e = findEntry(label);
        if (e!=nullptr){
            if (e->withLimits){
                if (value>=e->min && value<=e->max){
                    *(e->zone) = value;
                    result = true;
                } else {
                    LOGE("Value %s outsde limits", e->label);
                }
            } else {
                *(e->zone) = value;
                result = true;
            }
        } else {
            LOGE("Label '%s' not found", e->label);
        }
        return result;
    }

    // -- widget's layouts
    virtual void openTabBox(const char* label) {}
    virtual void openHorizontalBox(const char* label) {}
    virtual void openVerticalBox(const char* label) {}
    virtual void closeBox() {}

    // -- active widgets
    virtual void addButton(const char* label, FAUSTFLOAT* zone) {
        addEntry(label, zone);
    }
    virtual void addCheckButton(const char* label, FAUSTFLOAT* zone) {
        addEntry(label, zone);
    }
    virtual void addVerticalSlider(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step) {
        addEntry(label, zone, true, min, max);
        *zone = init;
    }
    virtual void addHorizontalSlider(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step) {
        addEntry(label, zone, true, min, max);
        *zone = init;
    }
    virtual void addNumEntry(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step) {}

    // -- passive widgets
    virtual void addHorizontalBargraph(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max) {}
    virtual void addVerticalBargraph(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max) {}

    // -- soundfiles
    virtual void addSoundfile(const char* label, const char* filename, Soundfile** sf_zone) {}

    // -- metadata declarations
    virtual void declare(FAUSTFLOAT* zone, const char* key, const char* val) {}

    protected:
        Vector<Entry> entries;

        Entry *findEntry(const char* name){
            Str nameStr(name);
            for (int j; j<entries.size();j++){
                if (nameStr.equals(entries[j].label)){
                    return &entries[j];
                }
            }
            return nullptr;
        }

        void addEntry(const char*label,FAUSTFLOAT* zone, bool withLimits=false, FAUSTFLOAT min=0, FAUSTFLOAT max=0){
            Entry e;
            e.label = label;
            e.zone = zone;
            e.withLimits = withLimits;
            if (withLimits){
                e.min = min;
                e.max = max;
                LOGI("Label: %s value: %f range: %f - %f", label, *zone, min, max);
            } else {
                LOGI("Label: %s value: %f", label, *zone);
            }
            entries.push_back(e);
        }

};


