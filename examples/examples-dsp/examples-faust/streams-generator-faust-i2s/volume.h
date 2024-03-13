/* ------------------------------------------------------------
author: "Grame"
copyright: "(c)GRAME 2006"
license: "BSD"
name: "volume"
version: "1.0"
Code generated with Faust 2.38.16 (https://faust.grame.fr)
Compilation options: -lang cpp -es 1 -mcd 16 -single -ftz 0 
------------------------------------------------------------ */

#ifndef  __mydsp_H__
#define  __mydsp_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>

#ifndef FAUSTCLASS 
#define FAUSTCLASS mydsp
#endif

#ifdef __APPLE__ 
#define exp10f __exp10f
#define exp10 __exp10
#endif

#if defined(_WIN32)
#define RESTRICT __restrict
#else
#define RESTRICT __restrict__
#endif


class mydsp : public dsp {
	
 private:
	
	FAUSTFLOAT fVslider0;
	float fRec0[2];
	int fSampleRate;
	
 public:
	
	void metadata(Meta* m) { 
		m->declare("author", "Grame");
		m->declare("basics_lib_name", "Faust Basic Element Library");
		m->declare("basics_lib_version", "0.1");
		m->declare("compilation_options", "-single -scal -inpl -fx -I /Users/pschatzmann/.FaustLive-CurrentSession-2.0/Libs -I /Users/pschatzmann/.FaustLive-CurrentSession-2.0/Examples");
		m->declare("compile_options", "-lang cpp -es 1 -mcd 16 -single -ftz 0 ");
		m->declare("copyright", "(c)GRAME 2006");
		m->declare("filename", "volume.dsp");
		m->declare("library_path", "volume");
		m->declare("license", "BSD");
		m->declare("name", "volume");
		m->declare("signals_lib_name", "Faust Signal Routing Library");
		m->declare("signals_lib_version", "0.0");
		m->declare("version", "1.0");
	}

	virtual int getNumInputs() {
		return 1;
	}
	virtual int getNumOutputs() {
		return 1;
	}
	
	static void classInit(int sample_rate) {
	}
	
	virtual void instanceConstants(int sample_rate) {
		fSampleRate = sample_rate;
	}
	
	virtual void instanceResetUserInterface() {
		fVslider0 = FAUSTFLOAT(0.0f);
	}
	
	virtual void instanceClear() {
		for (int l0 = 0; (l0 < 2); l0 = (l0 + 1)) {
			fRec0[l0] = 0.0f;
		}
	}
	
	virtual void init(int sample_rate) {
		classInit(sample_rate);
		instanceInit(sample_rate);
	}
	virtual void instanceInit(int sample_rate) {
		instanceConstants(sample_rate);
		instanceResetUserInterface();
		instanceClear();
	}
	
	virtual mydsp* clone() {
		return new mydsp();
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("volume");
		ui_interface->declare(&fVslider0, "1", "");
		ui_interface->addVerticalSlider("0x00", &fVslider0, FAUSTFLOAT(0.0f), FAUSTFLOAT(-70.0f), FAUSTFLOAT(4.0f), FAUSTFLOAT(0.100000001f));
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* output0 = outputs[0];
		float fSlow0 = (0.00100000005f * std::pow(10.0f, (0.0500000007f * float(fVslider0))));
		for (int i0 = 0; (i0 < count); i0 = (i0 + 1)) {
			fRec0[0] = (fSlow0 + (0.999000013f * fRec0[1]));
			output0[i0] = FAUSTFLOAT((float(input0[i0]) * fRec0[0]));
			fRec0[1] = fRec0[0];
		}
	}

};

#endif
