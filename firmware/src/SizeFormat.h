#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// "1,2 MB" / "1.2 MB": B, KB, MB ou GB com uma casa decimal (B sem casa), em buffer do chamador.
inline void formatSize(char* out, size_t cap, uint64_t bytes, char decimalSep) {
    uint64_t unit = 1;
    uint8_t u = 0;
    while (u < 3 && bytes >= unit * 1024) {
        unit *= 1024;
        ++u;
    }
    static const char* const kUnits[] = {"B", "KB", "MB", "GB"};
    if (u == 0) {
        snprintf(out, cap, "%u B", static_cast<unsigned>(bytes));
        return;
    }
    uint64_t tenths = (bytes * 10 + unit / 2) / unit;
    if (tenths >= 10240 && u < 3) { // 1023,96 KB arredonda para 1024,0: vira 1,0 da unidade seguinte
        unit *= 1024;
        ++u;
        tenths = (bytes * 10 + unit / 2) / unit;
    }
    snprintf(out, cap, "%u%c%u %s", static_cast<unsigned>(tenths / 10), decimalSep, static_cast<unsigned>(tenths % 10), kUnits[u]);
}
