#pragma once
#include <Arduino.h>
#include "OLEDDisplay.h"
class SSD1306Wire : public OLEDDisplay {
public:
    SSD1306Wire(uint8_t, int, int) { setGeometry(GEOMETRY_128_64); }
    bool connect() override { return true; }
    void display() override {}
    bool pixel(int x, int y) const { return buffer[x + (y / 8) * 128] >> (y % 8) & 1; }
private:
    int getBufferOffset() override { return 0; }
    void sendCommand(uint8_t) override {}
};
