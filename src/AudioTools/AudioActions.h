#pragma once
#include "Arduino.h"

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

  void add(int pin, void (*action)(), ActiveLogic activeLogicPar = ActiveLow) {
    if (maxIdx + 1 >= ACTIONS_MAX) {
      LOGE("Too many actions: please increase ACTIONS_MAX")
      return;
    }
    int pos = findPin(pin);
    if (pos != -1) {
      actions[pos] = action;
      activeLogic[pos] = activeLogicPar;
    } else {
      pins[maxIdx] = pin;
      actions[maxIdx] = action;
      activeLogic[maxIdx] = activeLogicPar;
      maxIdx++;
    }
    pinMode(pin, INPUT_PULLUP);
  }

  /**
   * @brief Execute all actions if the corresponding pin is low
   *
   */
  void processActions() {
    // execute action
    for (int j = 0; j < maxIdx; j++) {
      bool value = digitalRead(pins[j]);
      if (activeLogic[j] == ActiveChange) {
        if (value != lastState[j]) {
          // execute action
          actions[j]();
          lastState[j] = value;
        }
      } else {
        bool active = (activeLogic[j] == ActiveLow) ? !value : value;
        if (active &&
            (active != lastState[j] || millis() > debounceTimeout[j])) {
          // execute action
          actions[j]();
          lastState[j] = active;
          debounceTimeout[j] = millis() + DEBOUNCE_DELAY;
        }
      }
    }
  }

 protected:
  int maxIdx = 0;
  int pins[ACTIONS_MAX] = {0};
  ActiveLogic activeLogic[ACTIONS_MAX] = {ActiveLow};
  unsigned long debounceTimeout[ACTIONS_MAX] = {0};
  bool lastState[ACTIONS_MAX] = {0};
  void (*actions[ACTIONS_MAX])() = {0};

  int findPin(int pin) {
    for (int j = 0; j < maxIdx; j++) {
      if (pins[j] == pin) {
        return j;
      }
    }
    return -1;
  }
};
