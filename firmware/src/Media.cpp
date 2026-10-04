#include "Media.h"

#include <LittleFS.h>
#include <marmota.h>

#if MP3_SD
#include <SD.h>
#include <SPI.h>

#include "pins.h"
#endif

namespace media {

#if MP3_SD
constexpr uint32_t kSdHz = 10000000; // 4 e 10 MHz montam na breadboard; 20 MHz falha (fios longos)
constexpr uint8_t kSdMaxFiles = 1;   // cada slot custa ~4 KB de heap no mount; so a faixa tocando abre arquivo (pastas nao usam slot)
constexpr uint8_t kSdTries = 3;      // o primeiro comando apos ligar costuma falhar com crc error

bool mountSd() {
    for (uint8_t i = 0; i < kSdTries; ++i) {
        if (SD.begin(pins::kSdCs, SPI, kSdHz, "/sd", kSdMaxFiles))
            return true;
        SD.end();
        delay(20);
    }
    return false;
}
#endif

bool begin() {
    const bool flash = mrm::fsMount(); // config.json mora sempre na flash
#if MP3_SD
    SPI.begin(pins::kSdSck, pins::kSdMiso, pins::kSdMosi, pins::kSdCs);
    const bool sdOk = mountSd();
    return flash && sdOk;
#else
    return flash;
#endif
}

fs::FS& fs() {
#if MP3_SD
    return SD;
#else
    return LittleFS;
#endif
}

const char* mountPoint() {
#if MP3_SD
    return "/sd";
#else
    return "/littlefs";
#endif
}

uint64_t freeBytes() {
    return totalBytes() - usedBytes();
}

uint64_t usedBytes() {
#if MP3_SD
    return SD.usedBytes();
#else
    return LittleFS.usedBytes();
#endif
}

bool sdPresent() {
#if MP3_SD
    if (SD.cardType() == CARD_NONE)
        return false;
    File root = SD.open("/"); // cardType() e guardado na montagem: abrir a raiz detecta o cartao retirado
    return root && root.isDirectory();
#else
    return false;
#endif
}

uint64_t totalBytes() {
#if MP3_SD
    return SD.totalBytes();
#else
    return LittleFS.totalBytes();
#endif
}

} // namespace media
