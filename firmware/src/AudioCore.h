#pragma once

#include <BtSource.h>
#include <Mp3Stream.h>

// O rádio e o decoder: um de cada, compartilhados pelos arquivos Audio*.cpp.
namespace audio {
extern mrm::BtSource bt;
extern mrm::Mp3Stream stream;
} // namespace áudio
