#include "Media.h"

#include <LittleFS.h>
#include <marmota.h>

#if MP3_SD
#include <SD.h>
#include <SPI.h>

#include "pins.h"
#endif

namespace media {

bool begin() {
    const bool flash = mrm::fsMount(); // config.json mora sempre na flash
#if MP3_SD
    SPI.begin(pins::kSdSck, pins::kSdMiso, pins::kSdMosi, pins::kSdCs);
    return flash && SD.begin(pins::kSdCs);
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
    return SD.cardType() != CARD_NONE;
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
