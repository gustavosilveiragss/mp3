#pragma once
#include <Arduino.h>

namespace pins {

constexpr uint8_t kSdSck = 18;
constexpr uint8_t kSdMosi = 23;
constexpr uint8_t kSdMiso = 19;
constexpr uint8_t kSdCs = 17;

constexpr uint8_t kOledSda = 21;
constexpr uint8_t kOledScl = 22;

constexpr uint8_t kKeyA = 33;
constexpr uint8_t kKeyB = 32;

constexpr uint8_t kLed = 16;
constexpr uint8_t kBattery = 35;

} // namespace pins
