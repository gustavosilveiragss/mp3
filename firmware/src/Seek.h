#pragma once

#include <cstdint>

// Contas da busca dentro da faixa, sem Arduino para rodar no host contra MP3 reais.
namespace seek {

constexpr uint32_t kTapMs = 10000;       // toque de A ou B no modo busca
constexpr uint32_t kHoldMs = 30000;      // a cada repetição (150 ms) de A ou B segurado
constexpr uint32_t kCommitDelayMs = 350; // sem tecla por isso, aplica o destino (segurar soma sem reabrir)
constexpr uint32_t kModeIdleMs = 4000;   // sem tecla por isso, o modo busca fecha
constexpr uint32_t kTailMs = 3000;       // buscar além do fim cai aqui, para ouvir o fim e seguir para a próxima

// durationMs 0 = ainda desconhecida (faixa não carregada): só limita embaixo.
inline uint32_t clampMs(int64_t ms, uint32_t durationMs) {
    if (ms < 0)
        return 0;

    if (durationMs == 0)
        return ms > UINT32_MAX ? UINT32_MAX : uint32_t(ms);
    const uint32_t last = durationMs > kTailMs ? durationMs - kTailMs : 0;
    return ms > last ? last : uint32_t(ms);
}

} // namespace seek
