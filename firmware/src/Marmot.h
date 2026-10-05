#pragma once

#include <marmota.h>

namespace ui {

// Marmota saindo da toca, balancando a cabeça. top e o topo do sprite na pose do meio, eixo na coluna cx.
void marmot(mrm::Panel& o, int16_t cx, int16_t top, uint32_t elapsedMs);

} // namespace ui
