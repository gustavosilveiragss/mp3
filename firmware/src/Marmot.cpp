#include "Marmot.h"

#include <Gfx.h>

#include "MarmotIcon.h"

namespace ui {

namespace {

using art::kBurrowHalf;
using art::kBurrowRy;

// Ciclo de 1 s com tres estados; ficar parado nas pontas e o que da o ritmo de cabeca balancando.
// O ciclo termina no meio, entao o fim do splash (3800 ms) nunca corta a cabeca fora do lugar.
constexpr uint32_t kCycleMs = 1000;
constexpr uint32_t kMidAt = 300;
constexpr uint32_t kUpAt = 500;
constexpr uint32_t kBackAt = 800;

constexpr int16_t kBobPx = 1; // por lado: 2 px de pico a pico

int16_t bob(uint32_t elapsedMs) {
    const uint32_t t = elapsedMs % kCycleMs;
    if (t < kMidAt)
        return 1;
    if (t >= kUpAt && t < kBackAt)
        return -1;
    return 0;
}

// Contorno de 1 px da elipse cheia: a ponta de cada linha mais o trecho que a linha vizinha nao cobre.
bool rimLit(int16_t r, int16_t k) {
    if (k > kBurrowHalf[r])
        return false;
    if (r == 0 || r == 2 * kBurrowRy)
        return true;
    return k == kBurrowHalf[r] || k > min(kBurrowHalf[r - 1], kBurrowHalf[r + 1]);
}

// Pinta a linha r da toca fora de [hideLo, hideHi], o trecho atras da cabeca.
void rimRow(mrm::Panel& o, int16_t cx, int16_t y, int16_t r, int16_t hideLo, int16_t hideHi) {
    for (int16_t dx = -kBurrowHalf[r]; dx <= kBurrowHalf[r]; ++dx) {
        const int16_t x = cx + dx;
        if (rimLit(r, abs(dx)) && (x < hideLo || x > hideHi))
            o.setPixel(x, y);
    }
}

// A borda da frente passa na frente da cabeca: apaga o que do sprite cai abaixo dela.
void hideBelowRim(mrm::Panel& o, int16_t cx, int16_t rimTop, int16_t lo, int16_t hi, int16_t bottom) {
    o.setColor(BLACK);
    for (int16_t x = lo; x <= hi; ++x) {
        const int16_t k = abs(x - cx);
        int16_t r = 2 * kBurrowRy;
        while (r >= 0 && k > kBurrowHalf[r])
            --r;
        const int16_t y = rimTop + r + 1;
        if (r >= 0 && bottom > y)
            o.fillRect(x, y, 1, bottom - y);
    }
    o.setColor(WHITE);
}

} // namespace

void marmot(mrm::Panel& o, int16_t cx, int16_t top, uint32_t elapsedMs) {
    const int16_t side = bob(elapsedMs); // 1 px de lado junto com a subida, a inclinacao do balanco
    const int16_t y = top + side * kBobPx;
    const int16_t x = cx - art::kMarmot.w / 2 + side;
    const int16_t lo = x + art::kMarmotCup; // o corpo, sem as conchas
    const int16_t hi = x + art::kMarmot.w - 1 - art::kMarmotCup;
    const int16_t rimTop = top + art::kBurrowRow - kBurrowRy;

    o.setColor(WHITE);
    for (int16_t r = 0; r < kBurrowRy; ++r)
        rimRow(o, cx, rimTop + r, r, lo, hi);
    mrm::gfx::drawIcon(o, x, y, art::kMarmot);
    hideBelowRim(o, cx, rimTop, lo, hi, y + art::kMarmot.h);
    for (int16_t r = kBurrowRy; r <= 2 * kBurrowRy; ++r)
        rimRow(o, cx, rimTop + r, r, 1, 0); // intervalo vazio: a borda da frente aparece inteira
}

} // namespace ui
