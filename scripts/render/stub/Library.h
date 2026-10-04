#pragma once
#include <Arduino.h>
struct Library {
    static constexpr uint8_t kMaxPlaylists = 8;
    uint8_t n = 0;
    const char* names[kMaxPlaylists] = {};
    uint16_t counts[kMaxPlaylists] = {};
    uint8_t playlistCount() const { return n; }
    const char* playlistName(uint8_t i) const { return names[i]; }
    uint16_t trackCount(uint8_t i) const { return counts[i]; }
};
