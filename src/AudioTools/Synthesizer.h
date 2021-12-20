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
class AbstractSynthesizerChannel {
    public:
        virtual AbstractSynthesizerChannel* clone() = 0;
        /// Start the sound generation
        virtual void begin(AudioBaseInfo config);
        /// Checks if the ADSR is still active - and generating sound
        virtual bool isActive() = 0;
        /// Provides the key on event to ADSR to start the sound
        virtual void keyOn(int nte, float tgt) = 0;
        /// Provides the key off event to ADSR to stop the sound
        virtual void keyOff() = 0;
        /// Provides the next sample
        virtual int16_t readSample()=0;
        /// Provides the actual midi note that is played
        virtual int note() = 0 ;
};

/**
 * @brief Default implementation for a Channel
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class DefaultSynthesizerChannel : public AbstractSynthesizerChannel {
    public:
        DefaultSynthesizerChannel() = default;

        DefaultSynthesizerChannel(DefaultSynthesizerChannel &ch) {
            begin(ch.config);
        };
        
        virtual ~DefaultSynthesizerChannel() {
            LOGD(LOG_METHOD);
            if (shutdown_callback!=nullptr){
                shutdown_callback(ref);
            }
            if (p_generator!=nullptr){
                delete p_generator;
            }
            if (p_adsr!=nullptr){
                delete p_adsr;
            }
            if (p_audio_effects!=nullptr){
                delete p_audio_effects;
            }
        };

        DefaultSynthesizerChannel *clone() override {
            LOGD(LOG_METHOD);
            return new DefaultSynthesizerChannel(*this);
        }

        void setupCallback(void (*setup_callback)(AudioBaseInfo config), void(*shutdown_callback)(void *)=nullptr, void* ref=nullptr) {
            LOGI(LOG_METHOD);
            this->setup_callback = setup_callback;
            this->shutdown_callback = shutdown_callback;
            this->ref = ref;
        }

        virtual void begin(AudioBaseInfo config) override {
            LOGI(LOG_METHOD);
            this->config = config;
            if (setup_callback==nullptr){
                config.logInfo();
                p_generator = new SineWaveGenerator<int16_t>();
                p_audio_effects = new AudioEffects();
                p_adsr = new ADSRGain(0.0001, 0.0001, 0.8, 0.0005);
                p_audio_effects->setInput(*p_generator);
                p_audio_effects->addEffect(p_adsr);
            } else {
                setup_callback(config);
            }

            // start the generator if not done yet
            if (p_generator!=nullptr && !p_generator->isActive()){
                LOGI("Starting generator");
                p_generator->begin(config);
            }
        }

        void setGenerator(SoundGenerator<int16_t> *g){
            LOGD(LOG_METHOD);
            p_generator = g;
        }

        void setAudioEffects(AudioEffects *effects){
            LOGD(LOG_METHOD);
            p_audio_effects = effects;
        }

        virtual bool isActive() override{
            return p_adsr != nullptr ? p_adsr->isActive() : false;
        }

        /// start to play a note - note expects the frequency of the note!
        virtual void keyOn(int note, float tgt) override{
            if (p_generator!=nullptr){
                p_generator->setFrequency(note);
            }
            if (p_adsr!=nullptr){
                actual_note = note;
                p_adsr->keyOn(tgt);  
            } 
        }

        virtual void keyOff() override{
            LOGD(LOG_METHOD);
            if (p_adsr!=nullptr){
                p_adsr->keyOff();
            } else {
                LOGE("p_adsr is null")
            }
        }

        virtual int16_t readSample() override{
            return p_audio_effects != nullptr ? p_audio_effects->readSample() : 0;
        }

        virtual int note() override {
            return actual_note;
        }

    protected:
        AudioBaseInfo config;
        int actual_note = 0;
        SoundGenerator<int16_t>* p_generator=nullptr;
        AudioEffects* p_audio_effects = nullptr;
        ADSRGain *p_adsr = nullptr;
        void (*setup_callback)(AudioBaseInfo config) = nullptr;
        void (*shutdown_callback)(void*ref) = nullptr;
        void *ref = nullptr;

};

/**
 * @brief Arduino GPIO pin to note assossiation
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
            defaultChannel = new DefaultSynthesizerChannel();
        }

        Synthesizer(AbstractSynthesizerChannel *ch){
            defaultChannel = ch;
        }

        ~Synthesizer(){
            LOGD(LOG_METHOD);
            for (int j=0;j<channels.size();j++){
                delete channels[j];
                channels[j] = nullptr;
            }

#ifdef USE_MIDI
            delete p_synth_action;
#endif
       }

        void begin(AudioBaseInfo config) {
            LOGI(LOG_METHOD);
            this->cfg = config;
            SoundGenerator<int16_t>::begin(config);
            // provide config to defaut
            defaultChannel->begin(config);
        }

        void keyOn(int note, float tgt=0){
            LOGI("keyOn: %d", note);
            AbstractSynthesizerChannel *channel = getFreeChannel();
            if (channel!=nullptr){
                channel->keyOn(note, tgt);
            } else {
                LOGW("No channel available for %d",note);
            }
        }

        void keyOff(int note){
            LOGI("keyOff: %d", note);
            AbstractSynthesizerChannel *channel = getNoteChannel(note);
            if (channel!=nullptr){
                channel->keyOff();
            }
        }

        /// Provides mixed samples of all channels
        int16_t readSample() override {
            float total = 0;
            uint16_t count = 0;
            // calculate sum of all channels
            for (int j=0;j<channels.size();j++){
                if (channels[j]->isActive()){
                    count++;
                    total += channels[j]->readSample();
                }
            }
            // prevent divide by zero
            int16_t result = 0;
            if (count>0){
                result = 0.9 * total / count;
            }
            return result;
        }

        /// Assigns pins to notes - the last SynthesizerKey is marked with an entry containing the note <= 0 
        void setKeys(AudioActions &actions, SynthesizerKey* p_keys, AudioActions::ActiveLogic activeValue){
            while (p_keys->note > 0){
                actions.add(p_keys->pin, callbackKeyOn, callbackKeyOff, activeValue , new KeyParameter(this, p_keys->note)); 
                p_keys++;
            }
        }

        /// Defines the midi name
        void setMidiName(const char* name){
            midi_name = name;
        }

    protected:
        AudioBaseInfo cfg;
        AbstractSynthesizerChannel* defaultChannel;
        Vector<AbstractSynthesizerChannel*> channels;
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
                    int frq = MidiCommon::noteToFrequency(note);
                    float vel = 1.0/127.0 * velocity;
                    synth->keyOn(frq, vel);
                }
                void onNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) {
                    int frq = MidiCommon::noteToFrequency(note);
                    synth->keyOff(frq);
                }
                void onControlChange(uint8_t channel, uint8_t controller, uint8_t value) {}
                void onPitchBend(uint8_t channel, uint8_t value) {}
        };

        SynthAction *p_synth_action = new SynthAction(this);
        // Midi BLE Server
        MidiBleServer ble = MidiBleServer(midi_name, p_synth_action);

#endif

        // gets the channel for the indicated note
        AbstractSynthesizerChannel *getNoteChannel(int note){
            LOGI("getNoteChannel: %d", note);
            for (int j=0;j<channels.size();j++){
                if (channels[j]->note() == note){
                    return channels[j];
                }
            }
            return nullptr;
        }

        // gets a free channel
        AbstractSynthesizerChannel *getFreeChannel(){
            LOGI("getFreeChannel");
            for (int j=0;j<channels.size();j++){
                if (!channels[j]->isActive()){
                    return channels[j];
                }
            }
            LOGI("No free channel found: Adding new channel");
            AbstractSynthesizerChannel* ch = defaultChannel->clone();
            channels.push_back(ch);
            return ch;
        }

        static void callbackKeyOn(bool active, int pin, void* ref){
            LOGI(LOG_METHOD);
            KeyParameter* par = (KeyParameter*)ref;
            if (par !=nullptr && par->p_synthesizer!=nullptr){
                par->p_synthesizer->keyOn(par->note);
            } else {
                LOGE("callbackKeyOn: unexpected null")
            }
        }

        static void callbackKeyOff(bool active, int pin, void* ref){
            LOGI(LOG_METHOD);
            KeyParameter* par = (KeyParameter*)ref;
            if (par !=nullptr){
                if (par->p_synthesizer!=nullptr){
                    par->p_synthesizer->keyOff(par->note);
                } else {
                    LOGE("callbackKeyOff: p_synthesizer is null");               
                }
            } else {
                LOGE("callbackKeyOff: ref is null");               
            }
        }
};

} // namespace

