/* ------------------------------------------------------------
copyright: "(c)Romain Michon, CCRMA (Stanford University), GRAME"
license: "MIT"
name: "FluteMIDI"
Code generated with Faust 2.40.0 (https://faust.grame.fr)
Compilation options: -lang cpp -mem -es 1 -mcd 16 -single -ftz 0
------------------------------------------------------------ */

#ifndef  __mydsp_H__
#define  __mydsp_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

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

class mydspSIG0 {
	
  private:
	
	int iVec0[2];
	int iRec22[2];
	
  public:
	
	int getNumInputsmydspSIG0() {
		return 0;
	}
	int getNumOutputsmydspSIG0() {
		return 1;
	}
	
	void instanceInitmydspSIG0(int sample_rate) {
		for (int l2 = 0; l2 < 2; l2 = l2 + 1) {
			iVec0[l2] = 0;
		}
		for (int l3 = 0; l3 < 2; l3 = l3 + 1) {
			iRec22[l3] = 0;
		}
	}
	
	void fillmydspSIG0(int count, float* table) {
		for (int i1 = 0; i1 < count; i1 = i1 + 1) {
			iVec0[0] = 1;
			iRec22[0] = (iVec0[1] + iRec22[1]) % 65536;
			table[i1] = std::sin(9.58738019e-05f * float(iRec22[0]));
			iVec0[1] = iVec0[0];
			iRec22[1] = iRec22[0];
		}
	}

};

static mydspSIG0* newmydspSIG0(dsp_memory_manager* manager) { return (mydspSIG0*)new(manager->allocate(sizeof(mydspSIG0))) mydspSIG0(); }
static void deletemydspSIG0(mydspSIG0* dsp, dsp_memory_manager* manager) { dsp->~mydspSIG0(); manager->destroy(dsp); }

static float* ftbl0mydspSIG0 = 0;
static float mydsp_faustpower2_f(float value) {
	return value * value;
}

class mydsp : public dsp {
	
 private:
	
	FAUSTFLOAT fHslider0;
	int* iRec15;
	float* fRec21;
	FAUSTFLOAT fHslider1;
	int fSampleRate;
	float fConst1;
	FAUSTFLOAT fHslider2;
	float* fRec23;
	FAUSTFLOAT fButton0;
	FAUSTFLOAT fHslider3;
	float* fVec1;
	FAUSTFLOAT fHslider4;
	FAUSTFLOAT fHslider5;
	float* fRec24;
	float fConst5;
	int* iRec26;
	float fConst6;
	float fConst7;
	float fConst8;
	float* fRec25;
	float* fRec27;
	int IOTA0;
	float* fRec28;
	float fConst9;
	float fConst10;
	FAUSTFLOAT fHslider6;
	FAUSTFLOAT fHslider7;
	float* fRec29;
	float fConst11;
	FAUSTFLOAT fHslider8;
	float fConst12;
	float* fRec30;
	float* fVec2;
	float* fVec3;
	float* fVec4;
	float* fRec20;
	float* fRec11;
	float* fRec7;
	float* fRec3;
	float* fRec1;
	float* fRec2;
	float* fRec0;
	
 public:
	static dsp_memory_manager* fManager;
	
	void metadata(Meta* m) { 
		m->declare("basics.lib/name", "Faust Basic Element Library");
		m->declare("basics.lib/version", "0.5");
		m->declare("compile_options", "-lang cpp -mem -es 1 -mcd 16 -single -ftz 0");
		m->declare("copyright", "(c)Romain Michon, CCRMA (Stanford University), GRAME");
		m->declare("delays.lib/name", "Faust Delay Library");
		m->declare("delays.lib/version", "0.1");
		m->declare("description", "Simple MIDI-controllable flute physical model with physical parameters.");
		m->declare("filename", "fluteMIDI.dsp");
		m->declare("filters.lib/dcblocker:author", "Julius O. Smith III");
		m->declare("filters.lib/dcblocker:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/dcblocker:license", "MIT-style STK-4.3 license");
		m->declare("filters.lib/fir:author", "Julius O. Smith III");
		m->declare("filters.lib/fir:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/fir:license", "MIT-style STK-4.3 license");
		m->declare("filters.lib/iir:author", "Julius O. Smith III");
		m->declare("filters.lib/iir:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/iir:license", "MIT-style STK-4.3 license");
		m->declare("filters.lib/lowpass0_highpass1", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/lowpass0_highpass1:author", "Julius O. Smith III");
		m->declare("filters.lib/lowpass:author", "Julius O. Smith III");
		m->declare("filters.lib/lowpass:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/lowpass:license", "MIT-style STK-4.3 license");
		m->declare("filters.lib/name", "Faust Filters Library");
		m->declare("filters.lib/pole:author", "Julius O. Smith III");
		m->declare("filters.lib/pole:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/pole:license", "MIT-style STK-4.3 license");
		m->declare("filters.lib/tf2:author", "Julius O. Smith III");
		m->declare("filters.lib/tf2:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/tf2:license", "MIT-style STK-4.3 license");
		m->declare("filters.lib/tf2s:author", "Julius O. Smith III");
		m->declare("filters.lib/tf2s:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/tf2s:license", "MIT-style STK-4.3 license");
		m->declare("filters.lib/version", "0.3");
		m->declare("filters.lib/zero:author", "Julius O. Smith III");
		m->declare("filters.lib/zero:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/zero:license", "MIT-style STK-4.3 license");
		m->declare("license", "MIT");
		m->declare("maths.lib/author", "GRAME");
		m->declare("maths.lib/copyright", "GRAME");
		m->declare("maths.lib/license", "LGPL with exception");
		m->declare("maths.lib/name", "Faust Math Library");
		m->declare("maths.lib/version", "2.5");
		m->declare("name", "FluteMIDI");
		m->declare("noises.lib/name", "Faust Noise Generator Library");
		m->declare("noises.lib/version", "0.3");
		m->declare("oscillators.lib/name", "Faust Oscillator Library");
		m->declare("oscillators.lib/version", "0.3");
		m->declare("physmodels.lib/name", "Faust Physical Models Library");
		m->declare("physmodels.lib/version", "0.1");
		m->declare("platform.lib/name", "Generic Platform Library");
		m->declare("platform.lib/version", "0.2");
		m->declare("routes.lib/name", "Faust Signal Routing Library");
		m->declare("routes.lib/version", "0.2");
		m->declare("signals.lib/name", "Faust Signal Routing Library");
		m->declare("signals.lib/version", "0.1");
	}

	virtual int getNumInputs() {
		return 0;
	}
	virtual int getNumOutputs() {
		return 2;
	}
	
	static void classInit(int sample_rate) {
		mydspSIG0* sig0 = newmydspSIG0(fManager);
		sig0->instanceInitmydspSIG0(sample_rate);
		ftbl0mydspSIG0 = static_cast<float*>(fManager->allocate(262144));
		sig0->fillmydspSIG0(65536, ftbl0mydspSIG0);
		deletemydspSIG0(sig0, fManager);
	}
	
	static void classDestroy() {
		fManager->destroy(ftbl0mydspSIG0);
	}
	
	virtual void instanceConstants(int sample_rate) {
		fSampleRate = sample_rate;
		float fConst0 = std::min<float>(192000.0f, std::max<float>(1.0f, float(fSampleRate)));
		fConst1 = 1.0f / fConst0;
		float fConst2 = std::tan(6283.18555f / fConst0);
		float fConst3 = 1.0f / fConst2;
		float fConst4 = (fConst3 + 1.41421354f) / fConst2 + 1.0f;
		fConst5 = 0.0500000007f / fConst4;
		fConst6 = 1.0f / fConst4;
		fConst7 = (fConst3 + -1.41421354f) / fConst2 + 1.0f;
		fConst8 = 2.0f * (1.0f - 1.0f / mydsp_faustpower2_f(fConst2));
		fConst9 = 0.00882352982f * fConst0;
		fConst10 = 0.00147058826f * fConst0;
		fConst11 = 44.0999985f / fConst0;
		fConst12 = 1.0f - fConst11;
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = FAUSTFLOAT(0.5f);
		fHslider1 = FAUSTFLOAT(0.5f);
		fHslider2 = FAUSTFLOAT(5.0f);
		fButton0 = FAUSTFLOAT(0.0f);
		fHslider3 = FAUSTFLOAT(0.0f);
		fHslider4 = FAUSTFLOAT(0.90000000000000002f);
		fHslider5 = FAUSTFLOAT(1.0f);
		fHslider6 = FAUSTFLOAT(440.0f);
		fHslider7 = FAUSTFLOAT(0.0f);
		fHslider8 = FAUSTFLOAT(0.5f);
	}
	
	virtual void instanceClear() {
		for (int l0 = 0; l0 < 2; l0 = l0 + 1) {
			iRec15[l0] = 0;
		}
		for (int l1 = 0; l1 < 2; l1 = l1 + 1) {
			fRec21[l1] = 0.0f;
		}
		for (int l4 = 0; l4 < 2; l4 = l4 + 1) {
			fRec23[l4] = 0.0f;
		}
		for (int l5 = 0; l5 < 2; l5 = l5 + 1) {
			fVec1[l5] = 0.0f;
		}
		for (int l6 = 0; l6 < 2; l6 = l6 + 1) {
			fRec24[l6] = 0.0f;
		}
		for (int l7 = 0; l7 < 2; l7 = l7 + 1) {
			iRec26[l7] = 0;
		}
		for (int l8 = 0; l8 < 3; l8 = l8 + 1) {
			fRec25[l8] = 0.0f;
		}
		for (int l9 = 0; l9 < 2; l9 = l9 + 1) {
			fRec27[l9] = 0.0f;
		}
		IOTA0 = 0;
		for (int l10 = 0; l10 < 2048; l10 = l10 + 1) {
			fRec28[l10] = 0.0f;
		}
		for (int l11 = 0; l11 < 2; l11 = l11 + 1) {
			fRec29[l11] = 0.0f;
		}
		for (int l12 = 0; l12 < 2; l12 = l12 + 1) {
			fRec30[l12] = 0.0f;
		}
		for (int l13 = 0; l13 < 2; l13 = l13 + 1) {
			fVec2[l13] = 0.0f;
		}
		for (int l14 = 0; l14 < 2048; l14 = l14 + 1) {
			fVec3[l14] = 0.0f;
		}
		for (int l15 = 0; l15 < 2; l15 = l15 + 1) {
			fVec4[l15] = 0.0f;
		}
		for (int l16 = 0; l16 < 2; l16 = l16 + 1) {
			fRec20[l16] = 0.0f;
		}
		for (int l17 = 0; l17 < 2048; l17 = l17 + 1) {
			fRec11[l17] = 0.0f;
		}
		for (int l18 = 0; l18 < 2; l18 = l18 + 1) {
			fRec7[l18] = 0.0f;
		}
		for (int l19 = 0; l19 < 2048; l19 = l19 + 1) {
			fRec3[l19] = 0.0f;
		}
		for (int l20 = 0; l20 < 2; l20 = l20 + 1) {
			fRec1[l20] = 0.0f;
		}
		for (int l21 = 0; l21 < 2; l21 = l21 + 1) {
			fRec2[l21] = 0.0f;
		}
		for (int l22 = 0; l22 < 2; l22 = l22 + 1) {
			fRec0[l22] = 0.0f;
		}
	}
	
	virtual void init(int sample_rate) {}
	virtual void instanceInit(int sample_rate) {
		instanceConstants(sample_rate);
		instanceResetUserInterface();
		instanceClear();
	}
	
	virtual mydsp* clone() {
		return create();
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("flute");
		ui_interface->declare(0, "0", "");
		ui_interface->openHorizontalBox("midi");
		ui_interface->declare(&fHslider6, "0", "");
		ui_interface->declare(&fHslider6, "style", "knob");
		ui_interface->addHorizontalSlider("freq", &fHslider6, FAUSTFLOAT(440.0f), FAUSTFLOAT(50.0f), FAUSTFLOAT(1000.0f), FAUSTFLOAT(0.00999999978f));
		ui_interface->declare(&fHslider7, "1", "");
		ui_interface->declare(&fHslider7, "hidden", "1");
		ui_interface->declare(&fHslider7, "midi", "pitchwheel");
		ui_interface->declare(&fHslider7, "style", "knob");
		ui_interface->addHorizontalSlider("bend", &fHslider7, FAUSTFLOAT(0.0f), FAUSTFLOAT(-2.0f), FAUSTFLOAT(2.0f), FAUSTFLOAT(0.00999999978f));
		ui_interface->declare(&fHslider4, "2", "");
		ui_interface->declare(&fHslider4, "style", "knob");
		ui_interface->addHorizontalSlider("gain", &fHslider4, FAUSTFLOAT(0.899999976f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(0.00999999978f));
		ui_interface->declare(&fHslider5, "3", "");
		ui_interface->declare(&fHslider5, "style", "knob");
		ui_interface->addHorizontalSlider("envAttack", &fHslider5, FAUSTFLOAT(1.0f), FAUSTFLOAT(0.0f), FAUSTFLOAT(30.0f), FAUSTFLOAT(0.00999999978f));
		ui_interface->declare(&fHslider3, "4", "");
		ui_interface->declare(&fHslider3, "hidden", "1");
		ui_interface->declare(&fHslider3, "midi", "ctrl 64");
		ui_interface->declare(&fHslider3, "style", "knob");
		ui_interface->addHorizontalSlider("sustain", &fHslider3, FAUSTFLOAT(0.0f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(1.0f));
		ui_interface->closeBox();
		ui_interface->declare(0, "1", "");
		ui_interface->openHorizontalBox("otherParams");
		ui_interface->declare(&fHslider8, "0", "");
		ui_interface->declare(&fHslider8, "midi", "ctrl 1");
		ui_interface->declare(&fHslider8, "style", "knob");
		ui_interface->addHorizontalSlider("mouthPosition", &fHslider8, FAUSTFLOAT(0.5f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(0.00999999978f));
		ui_interface->declare(&fHslider2, "1", "");
		ui_interface->declare(&fHslider2, "style", "knob");
		ui_interface->addHorizontalSlider("vibratoFreq", &fHslider2, FAUSTFLOAT(5.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(10.0f), FAUSTFLOAT(0.00999999978f));
		ui_interface->declare(&fHslider1, "2", "");
		ui_interface->declare(&fHslider1, "style", "knob");
		ui_interface->addHorizontalSlider("vibratoGain", &fHslider1, FAUSTFLOAT(0.5f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(0.00999999978f));
		ui_interface->declare(&fHslider0, "3", "");
		ui_interface->declare(&fHslider0, "style", "knob");
		ui_interface->addHorizontalSlider("outGain", &fHslider0, FAUSTFLOAT(0.5f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(0.00999999978f));
		ui_interface->closeBox();
		ui_interface->declare(&fButton0, "2", "");
		ui_interface->addButton("gate", &fButton0);
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* output0 = outputs[0];
		FAUSTFLOAT* output1 = outputs[1];
		float fSlow0 = float(fHslider0);
		float fSlow1 = 0.0399999991f * float(fHslider1);
		float fSlow2 = fConst1 * float(fHslider2);
		float fSlow3 = std::min<float>(1.0f, float(fButton0) + float(fHslider3));
		float fSlow4 = 0.00100000005f * float(fHslider5);
		int iSlow5 = std::fabs(fSlow4) < 1.1920929e-07f;
		float fThen1 = std::exp(0.0f - fConst1 / ((iSlow5) ? 1.0f : fSlow4));
		float fSlow6 = ((iSlow5) ? 0.0f : fThen1);
		float fSlow7 = fSlow3 * float(fHslider4) * (1.0f - fSlow6);
		float fSlow8 = 340.0f / float(fHslider6);
		float fSlow9 = std::pow(2.0f, 0.0833333358f * float(fHslider7));
		int iSlow10 = fSlow3 == 0.0f;
		float fSlow11 = fConst11 * float(fHslider8);
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			iRec15[0] = 0;
			fRec21[0] = 0.284999996f * fRec20[1] + 0.699999988f * fRec21[1];
			float fRec19 = fRec21[0] + float(iRec15[1]);
			fRec23[0] = fSlow2 + fRec23[1] - std::floor(fSlow2 + fRec23[1]);
			fVec1[0] = fSlow3;
			fRec24[0] = fSlow7 + fSlow6 * fRec24[1];
			iRec26[0] = 1103515245 * iRec26[1] + 12345;
			fRec25[0] = 4.65661287e-10f * float(iRec26[0]) - fConst6 * (fConst7 * fRec25[2] + fConst8 * fRec25[1]);
			fRec27[0] = fRec1[1];
			fRec28[IOTA0 & 2047] = 0.949999988f * fRec27[1];
			float fTemp0 = float((fSlow3 == fVec1[1]) | iSlow10);
			fRec29[0] = fSlow9 * (1.0f - 0.999000013f * fTemp0) + 0.999000013f * fTemp0 * fRec29[1];
			float fTemp1 = fSlow8 / fRec29[0] + 0.270000011f;
			fRec30[0] = fSlow11 + fConst12 * fRec30[1];
			float fTemp2 = 0.400000006f * (fRec30[0] + -0.5f);
			float fTemp3 = fConst10 * fTemp1 * (fTemp2 + 0.270000011f);
			float fTemp4 = fTemp3 + -1.49999499f;
			int iTemp5 = int(fTemp4);
			int iTemp6 = int(std::min<float>(fConst9, float(std::max<int>(0, int(iTemp5))))) + 1;
			float fTemp7 = std::floor(fTemp4);
			float fTemp8 = fTemp3 + -1.0f - fTemp7;
			float fTemp9 = 0.0f - fTemp8;
			float fTemp10 = fTemp3 + -2.0f - fTemp7;
			float fTemp11 = 0.0f - 0.5f * fTemp10;
			float fTemp12 = fTemp3 + -3.0f - fTemp7;
			float fTemp13 = 0.0f - 0.333333343f * fTemp12;
			float fTemp14 = fTemp3 + -4.0f - fTemp7;
			float fTemp15 = 0.0f - 0.25f * fTemp14;
			float fTemp16 = fTemp3 - fTemp7;
			int iTemp17 = int(std::min<float>(fConst9, float(std::max<int>(0, int(iTemp5 + 1))))) + 1;
			float fTemp18 = 0.0f - fTemp10;
			float fTemp19 = 0.0f - 0.5f * fTemp12;
			float fTemp20 = 0.0f - 0.333333343f * fTemp14;
			int iTemp21 = int(std::min<float>(fConst9, float(std::max<int>(0, int(iTemp5 + 2))))) + 1;
			float fTemp22 = 0.0f - fTemp12;
			float fTemp23 = 0.0f - 0.5f * fTemp14;
			float fTemp24 = fTemp8 * fTemp10;
			int iTemp25 = int(std::min<float>(fConst9, float(std::max<int>(0, int(iTemp5 + 3))))) + 1;
			float fTemp26 = 0.0f - fTemp14;
			float fTemp27 = fTemp24 * fTemp12;
			int iTemp28 = int(std::min<float>(fConst9, float(std::max<int>(0, int(iTemp5 + 4))))) + 1;
			fVec2[0] = fRec28[(IOTA0 - iTemp6) & 2047] * fTemp9 * fTemp11 * fTemp13 * fTemp15 + fTemp16 * (fRec28[(IOTA0 - iTemp17) & 2047] * fTemp18 * fTemp19 * fTemp20 + 0.5f * fTemp8 * fRec28[(IOTA0 - iTemp21) & 2047] * fTemp22 * fTemp23 + 0.166666672f * fTemp24 * fRec28[(IOTA0 - iTemp25) & 2047] * fTemp26 + 0.0416666679f * fTemp27 * fRec28[(IOTA0 - iTemp28) & 2047]);
			float fTemp29 = (fSlow1 * ftbl0mydspSIG0[int(65536.0f * fRec23[0])] + fRec24[0] * (fConst5 * (fRec25[2] + fRec25[0] + 2.0f * fRec25[1]) + 1.0f)) - 0.5f * fVec2[1];
			float fTemp30 = 0.5f * fRec7[1] + std::max<float>(-1.0f, std::min<float>(1.0f, fTemp29 * (mydsp_faustpower2_f(fTemp29) + -1.0f)));
			fVec3[IOTA0 & 2047] = fTemp30;
			float fTemp31 = fConst10 * fTemp1 * (0.730000019f - fTemp2);
			float fTemp32 = fTemp31 + -1.49999499f;
			int iTemp33 = int(fTemp32);
			int iTemp34 = int(std::min<float>(fConst9, float(std::max<int>(0, int(iTemp33))))) + 1;
			float fTemp35 = std::floor(fTemp32);
			float fTemp36 = fTemp31 + -1.0f - fTemp35;
			float fTemp37 = 0.0f - fTemp36;
			float fTemp38 = fTemp31 + -2.0f - fTemp35;
			float fTemp39 = 0.0f - 0.5f * fTemp38;
			float fTemp40 = fTemp31 + -3.0f - fTemp35;
			float fTemp41 = 0.0f - 0.333333343f * fTemp40;
			float fTemp42 = fTemp31 + -4.0f - fTemp35;
			float fTemp43 = 0.0f - 0.25f * fTemp42;
			float fTemp44 = fTemp31 - fTemp35;
			int iTemp45 = int(std::min<float>(fConst9, float(std::max<int>(0, int(iTemp33 + 1))))) + 1;
			float fTemp46 = 0.0f - fTemp38;
			float fTemp47 = 0.0f - 0.5f * fTemp40;
			float fTemp48 = 0.0f - 0.333333343f * fTemp42;
			int iTemp49 = int(std::min<float>(fConst9, float(std::max<int>(0, int(iTemp33 + 2))))) + 1;
			float fTemp50 = 0.0f - fTemp40;
			float fTemp51 = 0.0f - 0.5f * fTemp42;
			float fTemp52 = fTemp36 * fTemp38;
			int iTemp53 = int(std::min<float>(fConst9, float(std::max<int>(0, int(iTemp33 + 3))))) + 1;
			float fTemp54 = 0.0f - fTemp42;
			float fTemp55 = fTemp52 * fTemp40;
			int iTemp56 = int(std::min<float>(fConst9, float(std::max<int>(0, int(iTemp33 + 4))))) + 1;
			fVec4[0] = fVec3[(IOTA0 - iTemp34) & 2047] * fTemp37 * fTemp39 * fTemp41 * fTemp43 + fTemp44 * (fVec3[(IOTA0 - iTemp45) & 2047] * fTemp46 * fTemp47 * fTemp48 + 0.5f * fTemp36 * fVec3[(IOTA0 - iTemp49) & 2047] * fTemp50 * fTemp51 + 0.166666672f * fTemp52 * fVec3[(IOTA0 - iTemp53) & 2047] * fTemp54 + 0.0416666679f * fTemp55 * fVec3[(IOTA0 - iTemp56) & 2047]);
			fRec20[0] = fVec4[1];
			float fRec16 = fRec19;
			float fRec17 = fRec20[0];
			float fRec18 = fRec20[0];
			fRec11[IOTA0 & 2047] = fRec16;
			float fRec12 = fTemp37 * fTemp39 * fTemp41 * fTemp43 * fRec11[(IOTA0 - iTemp34) & 2047] + fTemp44 * (fTemp46 * fTemp47 * fTemp48 * fRec11[(IOTA0 - iTemp45) & 2047] + 0.5f * fTemp36 * fTemp50 * fTemp51 * fRec11[(IOTA0 - iTemp49) & 2047] + 0.166666672f * fTemp52 * fTemp54 * fRec11[(IOTA0 - iTemp53) & 2047] + 0.0416666679f * fTemp55 * fRec11[(IOTA0 - iTemp56) & 2047]);
			float fRec13 = fRec17;
			float fRec14 = fRec18;
			fRec7[0] = fRec12;
			float fRec8 = fRec7[1];
			float fRec9 = fRec13;
			float fRec10 = fRec14;
			fRec3[IOTA0 & 2047] = fRec8;
			float fRec4 = fTemp9 * fTemp11 * fTemp13 * fTemp15 * fRec3[(IOTA0 - iTemp6) & 2047] + fTemp16 * (fTemp18 * fTemp19 * fTemp20 * fRec3[(IOTA0 - iTemp17) & 2047] + 0.5f * fTemp8 * fTemp22 * fTemp23 * fRec3[(IOTA0 - iTemp21) & 2047] + 0.166666672f * fTemp24 * fTemp26 * fRec3[(IOTA0 - iTemp25) & 2047] + 0.0416666679f * fTemp27 * fRec3[(IOTA0 - iTemp28) & 2047]);
			float fRec5 = fRec9;
			float fRec6 = fRec10;
			fRec1[0] = fRec4;
			fRec2[0] = fRec6;
			fRec0[0] = (fRec2[0] + 0.995000005f * fRec0[1]) - fRec2[1];
			float fTemp57 = fSlow0 * fRec0[0];
			output0[i0] = FAUSTFLOAT(fTemp57);
			output1[i0] = FAUSTFLOAT(fTemp57);
			iRec15[1] = iRec15[0];
			fRec21[1] = fRec21[0];
			fRec23[1] = fRec23[0];
			fVec1[1] = fVec1[0];
			fRec24[1] = fRec24[0];
			iRec26[1] = iRec26[0];
			fRec25[2] = fRec25[1];
			fRec25[1] = fRec25[0];
			fRec27[1] = fRec27[0];
			IOTA0 = IOTA0 + 1;
			fRec29[1] = fRec29[0];
			fRec30[1] = fRec30[0];
			fVec2[1] = fVec2[0];
			fVec4[1] = fVec4[0];
			fRec20[1] = fRec20[0];
			fRec7[1] = fRec7[0];
			fRec1[1] = fRec1[0];
			fRec2[1] = fRec2[0];
			fRec0[1] = fRec0[0];
		}
	}
	
	static void memoryInfo() {
		fManager->begin(24);
		// mydspSIG0
		fManager->info(16, 0, 0);
		// ftbl0mydspSIG0
		fManager->info(262144, 1, 0);
		// mydsp
		fManager->info(268, 42, 1);
		// iRec15
		fManager->info(8, 2, 2);
		// fRec21
		fManager->info(8, 3, 2);
		// fRec23
		fManager->info(8, 4, 2);
		// fVec1
		fManager->info(8, 2, 2);
		// fRec24
		fManager->info(8, 3, 2);
		// iRec26
		fManager->info(8, 3, 2);
		// fRec25
		fManager->info(12, 7, 3);
		// fRec27
		fManager->info(8, 2, 2);
		// fRec28
		fManager->info(8192, 5, 1);
		// fRec29
		fManager->info(8, 3, 2);
		// fRec30
		fManager->info(8, 3, 2);
		// fVec2
		fManager->info(8, 2, 2);
		// fVec3
		fManager->info(8192, 5, 1);
		// fVec4
		fManager->info(8, 2, 2);
		// fRec20
		fManager->info(8, 4, 2);
		// fRec11
		fManager->info(8192, 5, 1);
		// fRec7
		fManager->info(8, 3, 2);
		// fRec3
		fManager->info(8192, 5, 1);
		// fRec1
		fManager->info(8, 2, 2);
		// fRec2
		fManager->info(8, 3, 2);
		// fRec0
		fManager->info(8, 3, 2);
		fManager->end();
	}
	
	void memoryCreate() {
		iRec15 = static_cast<int*>(fManager->allocate(8));
		fRec21 = static_cast<float*>(fManager->allocate(8));
		fRec23 = static_cast<float*>(fManager->allocate(8));
		fVec1 = static_cast<float*>(fManager->allocate(8));
		fRec24 = static_cast<float*>(fManager->allocate(8));
		iRec26 = static_cast<int*>(fManager->allocate(8));
		fRec25 = static_cast<float*>(fManager->allocate(12));
		fRec27 = static_cast<float*>(fManager->allocate(8));
		fRec28 = static_cast<float*>(fManager->allocate(8192));
		fRec29 = static_cast<float*>(fManager->allocate(8));
		fRec30 = static_cast<float*>(fManager->allocate(8));
		fVec2 = static_cast<float*>(fManager->allocate(8));
		fVec3 = static_cast<float*>(fManager->allocate(8192));
		fVec4 = static_cast<float*>(fManager->allocate(8));
		fRec20 = static_cast<float*>(fManager->allocate(8));
		fRec11 = static_cast<float*>(fManager->allocate(8192));
		fRec7 = static_cast<float*>(fManager->allocate(8));
		fRec3 = static_cast<float*>(fManager->allocate(8192));
		fRec1 = static_cast<float*>(fManager->allocate(8));
		fRec2 = static_cast<float*>(fManager->allocate(8));
		fRec0 = static_cast<float*>(fManager->allocate(8));
	}
	
	void memoryDestroy() {
		fManager->destroy(iRec15);
		fManager->destroy(fRec21);
		fManager->destroy(fRec23);
		fManager->destroy(fVec1);
		fManager->destroy(fRec24);
		fManager->destroy(iRec26);
		fManager->destroy(fRec25);
		fManager->destroy(fRec27);
		fManager->destroy(fRec28);
		fManager->destroy(fRec29);
		fManager->destroy(fRec30);
		fManager->destroy(fVec2);
		fManager->destroy(fVec3);
		fManager->destroy(fVec4);
		fManager->destroy(fRec20);
		fManager->destroy(fRec11);
		fManager->destroy(fRec7);
		fManager->destroy(fRec3);
		fManager->destroy(fRec1);
		fManager->destroy(fRec2);
		fManager->destroy(fRec0);
	}

	static mydsp* create() {
		mydsp* dsp = new (fManager->allocate(sizeof(mydsp))) mydsp();
		dsp->memoryCreate();
		return dsp;
	}
	
	static void destroy(dsp* dsp) {
		static_cast<mydsp*>(dsp)->memoryDestroy();
		fManager->destroy(dsp);
	}

};

dsp_memory_manager* mydsp::fManager = nullptr;

#endif
