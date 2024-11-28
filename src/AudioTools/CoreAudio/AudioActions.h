#pragma once
#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/CoreAudio/AudioLogger.h"

#ifndef TOUCH_LIMIT
#define TOUCH_LIMIT 20
#endif

#ifndef DEBOUNCE_DELAY
#define DEBOUNCE_DELAY 500
#endif

#if defined(IS_MIN_DESKTOP)
extern "C" void pinMode(int, int);
extern "C" int digitalRead(int);
#endif

namespace audio_tools {

// global reference to access from static callback methods
class AudioActions;
static AudioActions *selfAudioActions = nullptr;

/**
 * @brief A simple class to assign functions to gpio pins e.g. to implement a
 * simple navigation control or volume control with buttons
 * @ingroup tools
 */
class AudioActions {
public:
  enum ActiveLogic : uint8_t {
    ActiveLow,
    ActiveHigh,
    ActiveChange,
    ActiveTouch
  };

  struct Action {
    Action() = default;
    virtual ~Action() {}
    int16_t pin = -1;
    void (*actionOn)(bool pinStatus, int pin, void *ref) = nullptr;
    void (*actionOff)(bool pinStatus, int pin, void *ref) = nullptr;
    void *ref = nullptr;
    unsigned long debounceTimeout = 0;
    ActiveLogic activeLogic = ActiveHigh;
    bool lastState = true;
    bool enabled = true;

    /// determines the value for the action
    int debounceDelayValue = DEBOUNCE_DELAY;
    int touchLimit = TOUCH_LIMIT;

    virtual int id() {
      return pin;
    }

    virtual bool readValue() {
#ifdef USE_TOUCH_READ
      bool result;
      if (this->activeLogic == ActiveTouch) {
        int value = touchRead(this->pin);
        result = value <= touchLimit;
        if (result) {
          // retry to confirm reading
          value = touchRead(this->pin);
          result = value <= touchLimit;
          LOGI("touch pin: %d value %d (limit: %d) -> %s", this->pin, value,
               touchLimit, result ? "true" : "false");
        }
      } else {
        result = digitalRead(this->pin);
      }
      return result;
#else
      return digitalRead(this->pin);
#endif
    }

    virtual void process() {
      if (this->enabled) {
        bool value = readValue();
        if (this->actionOn != nullptr && this->actionOff != nullptr) {
          // we have on and off action defined
          if (value != this->lastState) {
            //LOGI("processActions: case with on and off");
            // execute action -> reports active instead of pin state
            if ((value && this->activeLogic == ActiveHigh) ||
                (!value && this->activeLogic == ActiveLow)) {
              this->actionOn(true, this->pin, this->ref);
            } else {
              this->actionOff(false, this->pin, this->ref);
            }
            this->lastState = value;
          }
        } else if (this->activeLogic == ActiveChange) {
          bool active = value;
          // reports pin state
          if (value != this->lastState && millis() > this->debounceTimeout) {
            //LOGI("processActions: ActiveChange");
            //  execute action
            this->actionOn(active, this->pin, this->ref);
            this->lastState = value;
            this->debounceTimeout = millis() + debounceDelayValue;
          }
        } else {
          bool active = (this->activeLogic == ActiveLow) ? !value : value;
          if (active &&
              (active != this->lastState || millis() > this->debounceTimeout)) {
            // LOGI("processActions: %d Active %d - %d", this->pin, value,
            //  execute action
            this->actionOn(active, this->pin, this->ref);
            this->lastState = active;
            this->debounceTimeout = millis() + debounceDelayValue;
          }
        }
      }
    }
  };

  /// Default constructor
  AudioActions(bool useInterrupt = false) {
    selfAudioActions = this;
    setUsePinInterrupt(useInterrupt);
  }

  /// deletes all actions
  virtual ~AudioActions() {
    clear();
  }

  /// Adds an Action
  void add(Action &action){
    insertAction(action);      
  }

  /// Adds an action
  void add(int pin, void (*actionOn)(bool pinStatus, int pin, void *ref),
           ActiveLogic activeLogic = ActiveLow, void *ref = nullptr) {
    add(pin, actionOn, nullptr, activeLogic, ref);
  }

  /// Adds an action
  void add(int pin, void (*actionOn)(bool pinStatus, int pin, void *ref),
           void (*actionOff)(bool pinStatus, int pin, void *ref),
           ActiveLogic activeLogicPar = ActiveLow, void *ref = nullptr) {
    LOGI("ActionLogic::add pin: %d / logic: %d", pin, activeLogicPar);
    if (pin >= 0) {
      // setup pin mode
      setupPin(pin, activeLogicPar);

      // add value
      Action& action = *new Action();
      action.pin = pin;
      action.actionOn = actionOn;
      action.actionOff = actionOff;
      action.activeLogic = activeLogicPar;
      action.ref = ref;
      action.debounceDelayValue = debounceDelayValue;
      action.touchLimit = touchLimit;

      insertAction(action);      
    } else {
      LOGW("pin %d -> Ignored", pin);
    }
  }

  /// enable/disable pin actions
  void setEnabled(int pin, bool enabled) {
    Action *p_action = findAction(pin);
    if (p_action) {
      p_action->enabled = enabled;
    }
  }

  /**
   * @brief Execute all actions if the corresponding pin is low
   * To minimize the runtime: With each call we process a different pin
   */
  void processActions() {
    static int pos = 0;
    if (actions.empty())
      return;
    // execute action
    actions[pos]->process();
    pos++;
    if (pos >= actions.size()) {
      pos = 0;
    }
  }

  /// Execute all actions
  void processAllActions() {
    for (Action *action : actions) {
      action->process();
    }
  }

  /// Determines the action for the pin/id
  Action *findAction(int id) {
    for (Action *action : actions) {
      if (action->id() == id) {
        return action;
      }
    }
    return nullptr;
  }

  /// Determines the action for the pin/id
  int findActionIdx(int id) {
    int pos = 0;
    for (Action *action : actions) {
      if (action->id() == id) {
        return pos;
      }
      pos++;
    }
    return -1;
  }

  /// Defines the debounce delay
  void setDebounceDelay(int value) { debounceDelayValue = value; }
  /// Defines the touch limit (Default 20)
  void setTouchLimit(int value) { touchLimit = value; }
  /// Use interrupts instead of processActions() call in loop
  void setUsePinInterrupt(bool active) { use_pin_interrupt = active; }
  /// setup pin mode when true
  void setPinMode(bool active) { use_pin_mode = active; }

  void clear() {
    for (Action *act : actions){
      delete(act);
    }
    actions.reset();
  }

protected:
  int debounceDelayValue = DEBOUNCE_DELAY;
  int touchLimit = TOUCH_LIMIT;
  bool use_pin_interrupt = false;
  bool use_pin_mode = true;
  Vector<Action*> actions{0};

  void insertAction(Action& action){
    int idx = findActionIdx(action.id());
    if (idx >= 0) {
        // replace old action
        delete(actions[idx]);
        actions[idx] = &action;
    } else {
      // add new action
      actions.push_back(&action);
    }
  }

  static void audioActionsISR() { selfAudioActions->processAllActions(); }

  void setupPin(int pin, ActiveLogic logic) {
    // in the audio-driver library the pins are already set up
    if (use_pin_mode) {
      if (logic == ActiveLow) {
        pinMode(pin, INPUT_PULLUP);
        LOGI("pin %d -> INPUT_PULLUP", pin);
      } else {
        pinMode(pin, INPUT);
        LOGI("pin %d -> INPUT", pin);
      }
    }

#if !defined(IS_MIN_DESKTOP)
    if (use_pin_interrupt) {
      attachInterrupt(digitalPinToInterrupt(pin), audioActionsISR, CHANGE);
    }
#endif
  }
};

} // namespace audio_tools
