#pragma once
#include "Print.h"

namespace audio_tools {

/**
 * @brief Print sink that logs the length and a hex preview (first/last 10
 * bytes) of every write() call instead of actually outputting the data -
 * useful for verifying that video/audio codecs and containers are feeding
 * plausible data without needing a real display or audio device.
 */
class OutputTest : public Print {
 public:
    OutputTest() = default;

    size_t write(uint8_t c) override { return write(&c, 1); }

    size_t write(const uint8_t *buffer, size_t size) override {
        char hex_start[31] = {0};
        char hex_end[31] = {0};
        size_t start_count = size < 10 ? size : 10;
        for (size_t i = 0; i < start_count; ++i) {
            snprintf(hex_start + i * 3, 4, "%02X ", buffer[i]);
        }
        if (size > 10) {
            size_t end_count = (size - 10) < 10 ? (size - 10) : 10;
            for (size_t i = 0; i < end_count; ++i) {
                snprintf(hex_end + i * 3, 4, "%02X ", buffer[size - end_count + i]);
            }
        }
        LOGI("OutputTest: write() len=%u first=[%s] last=[%s]",
             (unsigned)size, hex_start, hex_end);
        return size;
    }
};

}  // namespace audio_tools
