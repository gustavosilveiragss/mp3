#pragma once

#include <FS.h>

namespace media {

// Monta a midia das musicas: LittleFS na flash, ou o cartao com -DMP3_SD=1.
bool begin();
fs::FS& fs();
const char* mountPoint(); // prefixo de fs() no VFS, para fopen direto
uint64_t freeBytes();
uint64_t totalBytes();
uint64_t usedBytes();
bool sdPresent(); // build com cartao e cartao montado

} // namespace media
