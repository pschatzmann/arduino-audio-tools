/* ------------------------------------------------------------
author: "Grame"
copyright: "(c)GRAME 2006"
license: "BSD"
name: "pitchShifter"
version: "1.0"
Code generated with Faust 2.68.1 (https://faust.grame.fr)
Compilation options: -lang cpp -ct 1 -es 1 -mcd 16 -single -ftz 0
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
	
	FAUSTFLOAT fHslider0;
	FAUSTFLOAT fHslider1;
	float fRec0[2];
	FAUSTFLOAT fHslider2;
	int IOTA0;
	float fVec0[131072];
	float fVec1[131072];
	int fSampleRate;
	
 public:
	mydsp() {}

	void metadata(Meta* m) { 
		m->declare("author", "Grame");
		m->declare("compile_options", "-lang cpp -ct 1 -es 1 -mcd 16 -single -ftz 0");
		m->declare("copyright", "(c)GRAME 2006");
		m->declare("delays.lib/name", "Faust Delay Library");
		m->declare("delays.lib/version", "1.1.0");
		m->declare("filename", "pitchShifter.dsp");
		m->declare("license", "BSD");
		m->declare("maths.lib/author", "GRAME");
		m->declare("maths.lib/copyright", "GRAME");
		m->declare("maths.lib/license", "LGPL with exception");
		m->declare("maths.lib/name", "Faust Math Library");
		m->declare("maths.lib/version", "2.6.0");
		m->declare("misceffects.lib/name", "Misc Effects Library");
		m->declare("misceffects.lib/version", "2.1.0");
		m->declare("name", "pitchShifter");
		m->declare("version", "1.0");
	}

	virtual int getNumInputs() {
		return 2;
	}
	virtual int getNumOutputs() {
		return 2;
	}
	
	static void classInit(int sample_rate) {
	}
	
	virtual void instanceConstants(int sample_rate) {
		fSampleRate = sample_rate;
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = FAUSTFLOAT(0.0f);
		fHslider1 = FAUSTFLOAT(1e+03f);
		fHslider2 = FAUSTFLOAT(1e+01f);
	}
	
	virtual void instanceClear() {
		for (int l0 = 0; l0 < 2; l0 = l0 + 1) {
			fRec0[l0] = 0.0f;
		}
		IOTA0 = 0;
		for (int l1 = 0; l1 < 131072; l1 = l1 + 1) {
			fVec0[l1] = 0.0f;
		}
		for (int l2 = 0; l2 < 131072; l2 = l2 + 1) {
			fVec1[l2] = 0.0f;
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
		ui_interface->openVerticalBox("Pitch Shifter");
		ui_interface->addHorizontalSlider("shift (semitones)", &fHslider0, FAUSTFLOAT(0.0f), FAUSTFLOAT(-12.0f), FAUSTFLOAT(12.0f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("window (samples)", &fHslider1, FAUSTFLOAT(1e+03f), FAUSTFLOAT(5e+01f), FAUSTFLOAT(1e+04f), FAUSTFLOAT(1.0f));
		ui_interface->addHorizontalSlider("xfade (samples)", &fHslider2, FAUSTFLOAT(1e+01f), FAUSTFLOAT(1.0f), FAUSTFLOAT(1e+04f), FAUSTFLOAT(1.0f));
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* input1 = inputs[1];
		FAUSTFLOAT* output0 = outputs[0];
		FAUSTFLOAT* output1 = outputs[1];
		float fSlow0 = std::pow(2.0f, 0.083333336f * float(fHslider0));
		float fSlow1 = float(fHslider1);
		float fSlow2 = 1.0f / float(fHslider2);
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			fRec0[0] = std::fmod(fSlow1 + (fRec0[1] + 1.0f - fSlow0), fSlow1);
			float fTemp0 = std::min<float>(fSlow2 * fRec0[0], 1.0f);
			float fTemp1 = 1.0f - fTemp0;
			float fTemp2 = float(input0[i0]);
			fVec0[IOTA0 & 131071] = fTemp2;
			float fTemp3 = fSlow1 + fRec0[0];
			int iTemp4 = int(fTemp3);
			int iTemp5 = std::min<int>(65537, std::max<int>(0, iTemp4 + 1));
			float fTemp6 = std::floor(fTemp3);
			float fTemp7 = fSlow1 + (fRec0[0] - fTemp6);
			float fTemp8 = 1.0f - fRec0[0];
			float fTemp9 = fTemp6 + fTemp8 - fSlow1;
			int iTemp10 = std::min<int>(65537, std::max<int>(0, iTemp4));
			int iTemp11 = int(fRec0[0]);
			int iTemp12 = std::min<int>(65537, std::max<int>(0, iTemp11 + 1));
			float fTemp13 = std::floor(fRec0[0]);
			float fTemp14 = fRec0[0] - fTemp13;
			float fTemp15 = fTemp13 + fTemp8;
			int iTemp16 = std::min<int>(65537, std::max<int>(0, iTemp11));
			output0[i0] = FAUSTFLOAT((fVec0[(IOTA0 - iTemp16) & 131071] * fTemp15 + fTemp14 * fVec0[(IOTA0 - iTemp12) & 131071]) * fTemp0 + (fVec0[(IOTA0 - iTemp10) & 131071] * fTemp9 + fTemp7 * fVec0[(IOTA0 - iTemp5) & 131071]) * fTemp1);
			float fTemp17 = float(input1[i0]);
			fVec1[IOTA0 & 131071] = fTemp17;
			output1[i0] = FAUSTFLOAT(fTemp0 * (fVec1[(IOTA0 - iTemp16) & 131071] * fTemp15 + fTemp14 * fVec1[(IOTA0 - iTemp12) & 131071]) + fTemp1 * (fTemp9 * fVec1[(IOTA0 - iTemp10) & 131071] + fTemp7 * fVec1[(IOTA0 - iTemp5) & 131071]));
			fRec0[1] = fRec0[0];
			IOTA0 = IOTA0 + 1;
		}
	}

};

#endif
