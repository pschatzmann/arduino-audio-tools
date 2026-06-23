#pragma once

#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/CoreAudio/AudioLogger.h"
#include "AudioTools/CoreAudio/AudioRuntime.h"
#include "AudioTools/CoreAudio/AudioTypes.h"

#ifndef TOUCH_LIMIT
#define TOUCH_LIMIT 20
#endif

#ifndef DEBOUNCE_DELAY
#define DEBOUNCE_DELAY 500
#endif

namespace audio_tools {

#if defined(IS_MIN_DESKTOP)
extern void pinMode(int, int);
extern int digitalRead(int);
#endif


/**
 * @brief A simple class to assign functions to gpio pins e.g. to implement a
 * simple navigation control or volume control with buttons
 * @ingroup tools
 */
class AudioActions {
 public:
  /// Defines the logic for the action execution
  enum ActiveLogic : uint8_t {
    ActiveLow,
    ActiveHigh,
    ActiveChange,
    ActiveTouch
  };

  /// Action definition per pin
  struct Action {
    Action() = default;
    virtual ~Action() {}
    digital_pin_t pin = GPIO_NONE;
    void (*actionOn)(bool pinStatus, digital_pin_t pin, void* ref) = nullptr;
    void (*actionOff)(bool pinStatus, digital_pin_t pin, void* ref) = nullptr;
    void* ref = nullptr;
    unsigned long debounceTimeout = 0;
    ActiveLogic activeLogic = ActiveHigh;
    bool lastState = true;
    bool enabled = true;

    /// determines the value for the action
    int debounceDelayValue = DEBOUNCE_DELAY;
    int touchLimit = TOUCH_LIMIT;
    bool (*read_cb)(digital_pin_t, void*) = nullptr;
    void* read_cb_ref = nullptr;

    // public  methods
    virtual int id() { return GPIO_TO_INT(pin); }
    virtual bool readValue() {
#if defined(USE_TOUCH_READ)
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
        result = readPin(this->pin);
      }
      return result;
#else
      return this->readPin(this->pin);
#endif
    }

    virtual void process() {
      if (this->enabled) {
        bool value = readValue();
        if (this->actionOn != nullptr && this->actionOff != nullptr) {
          // we have on and off action defined
          if (value != this->lastState) {
            // LOGI("processActions: case with on and off");
            //  execute action -> reports active instead of pin state
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
          if (value != this->lastState && millis() > this->debounceTimeout) {
            if (this->actionOn != nullptr)
              this->actionOn(active, this->pin, this->ref);
            this->lastState = value;
            this->debounceTimeout = millis() + debounceDelayValue;
          }
        } else {
          bool active = (this->activeLogic == ActiveLow) ? !value : value;
          if (active &&
              (active != this->lastState || millis() > this->debounceTimeout)) {
            if (this->actionOn != nullptr)
              this->actionOn(active, this->pin, this->ref);
            this->lastState = active;
            this->debounceTimeout = millis() + debounceDelayValue;
          }
        }
      }
    }

   protected:
    bool readPin(digital_pin_t pin) {
      if (read_cb) {
        return read_cb(pin, read_cb_ref);
      } else {
        return (bool)::digitalRead(pin);
      }
    }
  };

  /// Default constructor
  AudioActions(bool useInterrupt = false) {
    selfAudioActions = this;
    setUsePinInterrupt(useInterrupt);
  }

  /// deletes all actions
  virtual ~AudioActions() { clear(); }

  /// Adds an Action
  void add(Action& action) { insertAction(&action); }

  /// Adds an action
  void add(digital_pin_t pin,
           void (*actionOn)(bool pinStatus, digital_pin_t pin, void* ref),
           ActiveLogic activeLogic = ActiveLow, void* ref = nullptr) {
    add(pin, actionOn, nullptr, activeLogic, ref);
  }

  /// Adds an action
  void add(digital_pin_t pin,
           void (*actionOn)(bool pinStatus, digital_pin_t pin, void* ref),
           void (*actionOff)(bool pinStatus, digital_pin_t pin, void* ref),
           ActiveLogic activeLogicPar = ActiveLow, void* ref = nullptr) {
    LOGI("ActionLogic::add pin: %d / logic: %d", GPIO_TO_INT(pin),
         activeLogicPar);

    if (pin != GPIO_NONE) {
      // setup pin mode
      setupPin(pin, activeLogicPar);

      // add value
      Action* p_action = new Action();
      if (p_action == nullptr) {
        LOGE("Failed to allocate Action for pin %d", GPIO_TO_INT(pin));
        return;
      }
      p_action->pin = pin;
      p_action->actionOn = actionOn;
      p_action->actionOff = actionOff;
      p_action->activeLogic = activeLogicPar;
      p_action->ref = ref;
      p_action->debounceDelayValue = debounceDelayValue;
      p_action->touchLimit = touchLimit;
      p_action->read_cb = read_cb;
      p_action->read_cb_ref = read_cb_ref;

      insertAction(p_action);
    } else {
      LOGW("pin %d -> Ignored", GPIO_TO_INT(pin));
    }
  }

  /// enable/disable pin actions
  void setEnabled(digital_pin_t pin, bool enabled) {
    Action* p_action = findActionByPin(pin);
    if (p_action) {
      p_action->enabled = enabled;
    }
  }

  /**
   * @brief Execute all actions if the corresponding pin is low.
   * To minimize the runtime: With each call we process a different pin.
   * When interrupts are enabled, all actions are processed when a pin change
   * is detected.
   */
  void processActions() {
    if (actions.empty()) return;
    if (use_pin_interrupt) {
      if (!interrupt_pending) return;
      interrupt_pending = false;
      processAllActions();
    } else {
      static int pos = 0;
      auto action = actions[pos];
      if (action != nullptr && action->enabled) action->process();
      pos++;
      if (pos >= actions.size()) {
        pos = 0;
      }
    }
  }

  /// Execute all actions
  void processAllActions() {
    for (Action* action : actions) {
      if (action == nullptr || !action->enabled) continue;
      action->process();
    }
  }

  /// Determines the action for the pin/id
  Action* findActionById(int id) {
    for (Action* action : actions) {
      if (action != nullptr && action->id() == id) {
        return action;
      }
    }
    return nullptr;
  }

  Action* findActionByPin(digital_pin_t pin) {
    for (Action* action : actions) {
      if (action != nullptr && action->pin == pin) {
        return action;
      }
    }
    return nullptr;
  }

  /// Determines the action for the pin/id
  int findActionIdx(int id) {
    int pos = 0;
    for (Action* action : actions) {
      if (action == nullptr) continue;
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
  void setUsePinInterrupt(bool active) {
    use_pin_interrupt = active;
    for (Action* action : actions) {
      if (action == nullptr) continue;
      setupInterrupt(action->pin);
    }
  }
  /// setup pin mode when true
  void setPinMode(bool active) { use_pin_mode = active; }

  void clear() {
    for (Action* act : actions) {
      delete (act);
    }
    actions.reset();
  }

  /// Sets a callback function to read the pin state
  void setReadCallback(bool (*read_cb_par)(digital_pin_t, void*),
                       void* ref = nullptr) {
    read_cb = read_cb_par;
    read_cb_ref = ref;
  }

 protected:
  inline static AudioActions* selfAudioActions = nullptr;
  int debounceDelayValue = DEBOUNCE_DELAY;
  int touchLimit = TOUCH_LIMIT;
  bool use_pin_interrupt = false;
  bool use_pin_mode = true;
  volatile bool interrupt_pending = false;
  Vector<Action*> actions{0};
  bool (*read_cb)(digital_pin_t, void*) = nullptr;
  void* read_cb_ref = nullptr;

  void insertAction(Action* p) {
    if (p == nullptr) {
      LOGE("insertAction: refusing to add nullptr");
      return;
    }
    int idx = findActionIdx(p->id());
    if (idx >= 0) {
      delete (actions[idx]);
      actions[idx] = p;
    } else {
      actions.push_back(p);
    }
  }

#if defined(IS_ZEPHYR)
  static struct gpio_callback button_cb_data;

  static void audioActionsISRZephyr(const struct device* dev,
                                    struct gpio_callback* cb, uint32_t pins) {
    (void)dev;
    (void)cb;
    (void)pins;
    if (selfAudioActions != nullptr)
      selfAudioActions->interrupt_pending = true;
  }

  /**
   * @brief Setup GPIO interrupt for a given pin
   */
  void setupISR(digital_pin_t button) {
    int ret;

    ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_BOTH);
    if (ret != 0) {
      LOGE("Error %d: failed to enable interrupt", ret);
      return;
    }

    gpio_init_callback(&button_cb_data, audioActionsISRZephyr, BIT(button.pin));

    gpio_add_callback(button.port, &button_cb_data);
  }
#else
  static void audioActionsISR() {
    if (selfAudioActions != nullptr)
      selfAudioActions->interrupt_pending = true;
  }

#endif

  void setupInterrupt(digital_pin_t pin) {
    if (!use_pin_interrupt) return;
#if defined(ARDUINO) && (!defined(IS_MIN_DESKTOP) && !defined(IS_DESKTOP))
    attachInterrupt(digitalPinToInterrupt(pin), audioActionsISR, CHANGE);
#elif defined(IS_ZEPHYR)
    setupISR(pin);
#endif
  }

  void setupPin(digital_pin_t pin, ActiveLogic logic) {
    if (use_pin_mode) {
      if (logic == ActiveLow) {
        pinMode(pin, INPUT_PULLUP);
        LOGI("pin %d -> INPUT_PULLUP", GPIO_TO_INT(pin));
      } else {
        pinMode(pin, INPUT);
        LOGI("pin %d -> INPUT", GPIO_TO_INT(pin));
      }
    }
    setupInterrupt(pin);
  }
};

}  // namespace audio_tools
