#include "PortalSelfTest.h"

#if MP3_TEST_PORTAL

#include <Arduino.h>

#include "Navigator.h"

namespace {
Navigator* target = nullptr;
bool opened = false;
} // namespace

void PortalSelfTest::begin(Navigator& navigator) {
    target = &navigator;
}

void PortalSelfTest::update() {
    if (opened || !target || millis() < kOpenAfterMs)
        return;
    opened = true;
    target->startWifi();
}

#endif
