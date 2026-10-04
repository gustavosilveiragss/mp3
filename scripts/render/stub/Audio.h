#pragma once
#include <Arduino.h>
struct Audio {
    static constexpr uint8_t kMaxDevices = 8;
    struct Device {
        char name[33];
        int8_t rssi;
    };
    enum class Link : uint8_t { Off, Idle, Connecting, Connected };
};
