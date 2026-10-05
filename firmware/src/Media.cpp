#include "Media.h"

#include <SD.h>
#include <SPI.h>
#include <marmota.h>

#include "pins.h"

namespace media {

constexpr uint32_t kSdHz = 10000000; // 20 MHz falha com fios longos
constexpr uint8_t kSdMaxFiles = 1;   // cada slot custa ~4 KB de heap, só a faixa tocando abre arquivo
constexpr uint8_t kSdTries = 3;      // o primeiro comando após ligar costuma falhar com crc error

bool mountSd() {
    for (uint8_t i = 0; i < kSdTries; ++i) {
        if (SD.begin(pins::kSdCs, SPI, kSdHz, "/sd", kSdMaxFiles))
            return true;

        SD.end();
        delay(20);
    }

    return false;
}

bool begin() {
    const bool flash = mrm::fsMount(); // config.json mora sempre na flash
    SPI.begin(pins::kSdSck, pins::kSdMiso, pins::kSdMosi, pins::kSdCs);
    const bool sdOk = mountSd();
    return flash && sdOk;
}

fs::FS& fs() {
    return SD;
}

const char* mountPoint() {
    return "/sd";
}

uint64_t freeBytes() {
    return totalBytes() - usedBytes();
}

uint64_t usedBytes() {
    return SD.usedBytes();
}

bool sdPresent() {
    if (SD.cardType() == CARD_NONE)
        return false;
    File root = SD.open("/"); // cardType() e guardado na montagem: abrir a raiz detecta o cartão retirado
    return root && root.isDirectory();
}

uint64_t totalBytes() {
    return SD.totalBytes();
}

} // namespace media
