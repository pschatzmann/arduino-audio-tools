#pragma once

#include "AudioLogger.h"

namespace audio_tools {

    /**
     * @brief Helper class to debounce user input from a push button
     * @ingroup tools
     */
    class Debouncer {

    public:
        Debouncer(uint16_t timeoutMs = 5000, void* ref = nullptr) {
            setDebounceTimeout(timeoutMs);
            p_ref = ref;
        }

        void setDebounceTimeout(uint16_t timeoutMs) {
            ms = timeoutMs;
        }

        /// Prevents that the same method is executed multiple times within the indicated time limit
        bool debounce(void(*cb)(void* ref) = nullptr) {
            bool result = false;
            // signed-difference comparison so this keeps working correctly
            // across a millis() rollover (~49.7 days), unlike a plain
            // millis() > debounce_ms check
            if ((long)(millis() - debounce_ms) >= 0) {
                LOGI("accepted");
                if (cb != nullptr) cb(p_ref);
                // new time limit
                debounce_ms = millis() + ms;
                result = true;
            }
            else {
                LOGI("rejected");
            }
            return result;
        }

    protected:
        unsigned long debounce_ms = 0; // Debounce sensitive touch
        uint16_t ms;
        void* p_ref = nullptr;

    };

}