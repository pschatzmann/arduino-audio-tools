#pragma once 

#include "AudioConfig.h"
#include "AudioBasic/Collections.h"
#include "AudioBasic/Float16.h"

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#ifndef PSRAM_LIMIT
#define PSRAM_LIMIT 1024
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
        return e!=nullptr ? *(e->zone) :(FAUSTFLOAT) 0.0;
    }
    virtual bool setValue(const char* label, FAUSTFLOAT value){
        bool result = false;
        Entry* e = findEntry(label);
        if (e!=nullptr){
            if (e->withLimits){
                if (value>=e->min && value<=e->max){
                    *(e->zone) = value;
                    result = true;
                } else {
                    LOGE("Value '%s' outsde limits %f (%f-%f)", e->label, value, e->min, e->max);
                }
            } else {
                *(e->zone) = value;
                result = true;
            }
        } else {
            LOGE("Label '%s' not found", label);
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
    virtual void addNumEntry(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step) {
        addEntry(label, zone, true, min, max);
        *zone = init;
    }

    // -- passive widgets
    virtual void addHorizontalBargraph(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max) {}
    virtual void addVerticalBargraph(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max) {}

    // -- soundfiles
    virtual void addSoundfile(const char* label, const char* filename, Soundfile** sf_zone) {}

    // -- metadata declarations
    virtual void declare(FAUSTFLOAT* zone, const char* key, const char* val) {}

    /// checks if a label exists
    virtual bool exists(const char*label){
        return findEntry(label)!=nullptr;
    }

    /// Returns the number of label entries
    virtual size_t size() {
        return entries.size();
    }
    
    /// Returns the label at the indicated position. nullptr is returned if the index is too big
    const char* label(int idx){
        if (idx<size()){
            return entries[idx].label;
        }
        return nullptr;
    }

    protected:
        Vector<Entry> entries;

        Entry *findEntry(const char* name){
            Str nameStr(name);
            for (int j=0; j<entries.size();j++){
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

/**
 * @brief Memory manager which uses psram when it is available
 * 
 */
class dsp_memory_manager {
public:
    virtual ~dsp_memory_manager() {}

    /**
    * Inform the Memory Manager with the number of expected memory zones.
    * @param count - the number of memory zones
    */
    virtual void begin(size_t count){
        this->count = count;
        total = 0;
    }

    /**
    * Give the Memory Manager information on a given memory zone.
    * @param size - the size in bytes of the memory zone
    * @param reads - the number of Read access to the zone used to compute one frame
    * @param writes - the number of Write access to the zone used to compute one frame
    */
    virtual void info(size_t size, size_t reads, size_t writes) {
        LOGD("info %d", size);
        total+=size;
    }

    /**
    * Inform the Memory Manager that all memory zones have been described, 
    * to possibly start a 'compute the best allocation strategy' step.
    */
    virtual void end(){
#ifdef ESP32
        is_psram = total>2000 && ESP.getFreePsram()>0;
#endif
        LOGI("use PSRAM: %s", is_psram?"true":"false");
    }

    /**
    * Allocate a memory zone.
    * @param size - the memory zone size in bytes
    */
    virtual void* allocate(size_t size) {
        LOGD("allocate %d", size);
#ifdef ESP32
        void* result = is_psram && size > PSRAM_LIMIT ? ps_malloc(size) : malloc(size);
#else
        void* result = malloc(size);
#endif
        if (result!=nullptr){
            memset(result, size, 0);
        } else {
            LOGE("allocate %u bytes - failed", (unsigned) size);
        }
        return result;
    };
    

    /**
    * Destroy a memory zone.
    * @param ptr - the memory zone pointer to be deallocated
    */
    virtual void destroy(void* ptr) {
        LOGD("destroy");
        free(ptr);
    };

private:
    size_t count;
    size_t total;
    bool is_psram = false;


};



