#pragma once

#include <FS.h>

namespace media {

// Monta o cartão SD das músicas. O config.json fica na flash.
bool begin();
fs::FS& fs();
const char* mountPoint(); // prefixo de fs() no VFS, para fopen direto
uint64_t freeBytes();
uint64_t totalBytes();
uint64_t usedBytes();
bool sdPresent(); // cartão montado e presente

} // namespace media
