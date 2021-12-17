#pragma once
#include "Arduino.h"
#include "AudioTools/AudioLogger.h"

#ifndef ACTIONS_MAX
#define ACTIONS_MAX 20
#endif

#ifndef DEBOUNCE_DELAY
#define DEBOUNCE_DELAY 500
#endif

/**
 * @brief A simple class to assign Functions to Pins e.g. to implement a simple
 * navigation control or volume control with buttons 
 */
class AudioActions {
 public:
  enum ActiveLogic { ActiveLow, ActiveHigh, ActiveChange };

  void add(int pin, void (*actionOn)(bool pinStatus,int pin, void*ref), ActiveLogic activeLogic = ActiveLow, void* ref=nullptr) {
    add(pin, actionOn, nullptr, activeLogic, ref );
  }

  void add(int pin, void (*actionOn)(bool pinStatus,int pin, void*ref), void (*actionOff)(bool pinStatus,int pin, void*ref), ActiveLogic activeLogicPar = ActiveLow, void* ref=nullptr) {
    LOGI("ActionLogic::add pin: %d", pin);
    if (maxIdx + 1 >= ACTIONS_MAX) {
      LOGE("Too many actions: please increase ACTIONS_MAX")
      return;
    }

    pinMode(pin, INPUT_PULLUP);

    int pos = findPin(pin);
    if (pos != -1) {
      actions[pos].actionOn = actionOn;
      actions[pos].actionOff = actionOff;
      actions[pos].activeLogic = activeLogicPar;
      actions[pos].ref = ref;
    } else {
      actions[maxIdx].pin = pin;
      actions[maxIdx].actionOn = actionOn;
      actions[maxIdx].actionOff = actionOff;
      actions[maxIdx].activeLogic = activeLogicPar;
      actions[maxIdx].ref = ref;
      maxIdx++;
    }
  }

  /**
   * @brief Execute all actions if the corresponding pin is low
   *
   */
  void processActions() {
    // execute action
    for (int j = 0; j < maxIdx; j++) {
      Action *a = &(actions[j]);
      bool value = digitalRead(a->pin);
      if (a->actionOn!=nullptr && a->actionOff!=nullptr){
        // we have on and off action defined
        if (value != a->lastState) {
          LOGD("processActions: case with on and off");
          // execute action -> reports active instead of pin state
          if ((value && a->activeLogic==ActiveHigh)|| (!value && a->activeLogic==ActiveLow) ){
            a->actionOn(true, a->pin, a->ref);
          } else {
            a->actionOff(false, a->pin, a->ref);
          }
          a->lastState = value;
        }
      } else if (a->activeLogic == ActiveChange) {
        bool active = (a->activeLogic == ActiveLow) ? !value : value;
        // reports pin state
        if (value != a->lastState) {
          LOGD("processActions: ActiveChange");
          // execute action
          a->actionOn(active, a->pin, a->ref);
          a->lastState = value;
        }
      } else {
        bool active = (a->activeLogic == ActiveLow) ? !value : value;
        if (active && (active != a->lastState || millis() > a->debounceTimeout)) {
          LOGD("processActions: Active");
          // execute action
          a->actionOn(active, a->pin, a->ref);
          a->lastState = active;
          a->debounceTimeout = millis() + DEBOUNCE_DELAY;
        }
      }
    }
  }

  /// Defines the debounce delay
  void setDeboundDelay(int value){
    debounceDelayValue = value;
  }

 protected:
  int maxIdx = 0;
  int debounceDelayValue = DEBOUNCE_DELAY;

  struct Action {
    int pin;
    ActiveLogic activeLogic;
    void (*actionOn)(bool pinStatus, int pin, void*ref) = nullptr;
    void (*actionOff)(bool pinStatus, int pin, void*ref) = nullptr;
    void *ref=nullptr;
    unsigned long debounceTimeout;
    bool lastState;
  } actions[ACTIONS_MAX];

  int findPin(int pin) {
    for (int j = 0; j < maxIdx; j++) {
      if (actions[j].pin == pin) {
        return j;
      }
    }
    return -1;
  }
};
