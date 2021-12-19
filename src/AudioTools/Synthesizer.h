#pragma once

#include "AudioTools/AudioLogger.h"
#include "AudioTools/AudioTypes.h"
#include "AudioTools/Vector.h"
#include "AudioTools/AudioActions.h"
#include "AudioTools/SoundGenerator.h"
#include "AudioTools/AudioStreams.h"
#include "AudioEffects/AudioEffects.h"
#ifdef USE_MIDI
#include "Midi.h"
#endif

namespace audio_tools {

/**
 * @brief Defines the sound generation for one channel. A channel is used to process an indivudual key so 
 * that we can generate multiple notes at the same time.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AbstractSyntheizerChannel {
    public:
        virtual AbstractSyntheizerChannel* clone() = 0;
        virtual void begin(AudioBaseInfo config);
        virtual bool isActive() = 0;
        virtual void keyOn(int nte, float tgt) = 0;
        virtual void keyOff() = 0;
        virtual int16_t readSample()=0;
        virtual int note() = 0 ;
        virtual AudioEffects &audioEffects();
};

/**
 * @brief Default implementation for a Channel
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class DefaultSyntheizerChannel : public AbstractSyntheizerChannel {
    public:
        DefaultSyntheizerChannel() = default;

        DefaultSyntheizerChannel(DefaultSyntheizerChannel &ch) = default;
        
        DefaultSyntheizerChannel *clone() override {
            return new DefaultSyntheizerChannel(*this);
        }

        void setBeginCallback(void (*setup_callback)(AudioBaseInfo config)) {
            this->setup_callback = setup_callback;
        }

        virtual void begin(AudioBaseInfo config) override {
            if (setup_callback==nullptr){
                audio_effects.setInput(sine);
                audio_effects.addEffect(adsr);
                sine.begin(config, N_C3 );
            } else {
                setup_callback();
            }
        }

        virtual bool isActive() override{
            adsr_obj.isActive();
        }

        virtual void keyOn(int nte, float tgt) override{
            actual_note = nte;
            adsr_obj.keyOn(tgt);
        }

        virtual void keyOff() override{
            adsr_obj.keyOff();
        }

        virtual int16_t readSample() override{
            return audio_effects.readSample();
        }

        virtual int note() override {
            return actual_note;
        }

        virtual AudioEffects &audioEffects()override {
            return audio_effects;
        }

        virtual ADSR &adsr(){
            return adsr_obj;
        }

    protected:
        SineWaveGenerator<int16_t> sine;
        ADSR adsr_obj = ADSR(0.0001, 0.0001, 0.8, 0.0005);
        AudioEffects audio_effects;
        int actual_note = 0;
        void (*setup_callback)(AudioBaseInfo config) = nullptr;

};

/**
 * @brief pin to note assossiation
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
struct SynthesizerKey {
    int pin;
    int note;
};


/**
 * @brief A simple Synthesizer which can generate sound having multiple keys pressed. The main purpose
 * of this class is managing the synthezizer channels
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class Synthesizer : public SoundGenerator<int16_t> {
    public:
        Synthesizer() {
            defaultChannel = new DefaultSyntheizerChannel();
        }

        Synthesizer(AbstractSyntheizerChannel *ch){
            defaultChannel = ch;
        }

        ~Synthesizer(){
            for (int j=0;j<channels.size();j++){
                delete channels[j];
                channels[j] = nullptr;
            }

#ifdef USE_MIDI
            delete p_synth_action;
#endif
       }

        void begin(AudioBaseInfo config) {
            this->cfg = config;
        }

        void keyOn(int note, float tgt=0){
            LOGI("keyOn: %d", note);
            AbstractSyntheizerChannel *channel = getFreeChannel();
            if (channel!=nullptr){
                channel->keyOn(note, tgt);
            } else {
                LOGW("No channel available for %d",note);
            }
        }

        void keyOff(int note){
            LOGI("keyOff: %d", note);
            AbstractSyntheizerChannel *channel = getNoteChannel(note);
            if (channel!=nullptr){
                channel->keyOff();
            }
        }

        /// Provides mixed samples of all channels
        int16_t readSample() {
            int64_t total = 0;
            uint16_t count = 0;
            for (int j=0;j<channels.size();j++){
                if (!channels[j]->isActive()){
                    count++;
                    total += channels[j]->readSample();
                }
            }
            total = total / count;
            return total;
        }

        /// Provides access to the AudioEffects
        AudioEffects &audioEffects() {
            return defaultChannel->audioEffects();
        }

        /// Assigns pins to notes
        void setKeys(AudioActions &actions, SynthesizerKey* p_keys, AudioActions::ActiveLogic activeValue){
            while (p_keys->note > 0){
                actions.add(p_keys->pin, callbackKeyOn, callbackKeyOff,activeValue ,new KeyParameter(this, p_keys->note)); 
                p_keys++;
            }
        }

        /// Defines the midi name
        void setMidiName(const char* name){
            midi_name = name;
        }

    protected:
        AudioBaseInfo cfg;
        AbstractSyntheizerChannel* defaultChannel;
        Vector<AbstractSyntheizerChannel*> channels;
        const char* midi_name = "Synthesizer";

        struct KeyParameter {
            KeyParameter(Synthesizer* synth, int nte){
                p_synthesizer=synth;
                note = nte;
            };
            Synthesizer *p_synthesizer = nullptr;
            int note;
        };

#ifdef USE_MIDI

        /// Midi Support
        class SynthAction : public MidiAction {
            private:
                Synthesizer *synth=nullptr;
            public:
                SynthAction(Synthesizer *synth){
                    this->synth = synth;
                }
                void onNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
                    float v = 1.0/127.0 * velocity;
                    synth->keyOn(note, v);
                }
                void onNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) {
                    synth->keyOff(note);
                }
                void onControlChange(uint8_t channel, uint8_t controller, uint8_t value) {}
                void onPitchBend(uint8_t channel, uint8_t value) {}
        };

        SynthAction *p_synth_action = new SynthAction(this);
        // Midi BLE Server
        MidiBleServer ble = MidiBleServer(midi_name, p_synth_action);

#endif

        // gets the channel for the indicated note
        AbstractSyntheizerChannel *getNoteChannel(int note){
            for (int j=0;j<channels.size();j++){
                if (!channels[j]->note() == note){
                    return channels[j];
                }
            }
            return nullptr;
        }

        // gets a free channel
        AbstractSyntheizerChannel *getFreeChannel(){
            for (int j=0;j<channels.size();j++){
                if (!channels[j]->isActive()){
                    return channels[j];
                }
            }
            LOGI("adding new channel");
            AbstractSyntheizerChannel* ch = defaultChannel->clone();
            ch->begin(cfg);
            channels.push_back(ch);
            return ch;
        }

        static void callbackKeyOn(bool active, int pin, void* ptr){
            KeyParameter* par = (KeyParameter*)ptr;
            if (par !=nullptr && par->p_synthesizer!=nullptr){
                par->p_synthesizer->keyOn(par->note);
            } else {
                LOGE("callbackKeyOn: unexpected null")
            }
        }

        static void callbackKeyOff(bool active, int pin, void* ptr){
            KeyParameter* par = (KeyParameter*)ptr;
            if (par !=nullptr && par->p_synthesizer!=nullptr){
                par->p_synthesizer->keyOff(par->note);
            } {
                LOGE("callbackKeyOff: unexpected null")                
            }
        }


};

} // namespace

