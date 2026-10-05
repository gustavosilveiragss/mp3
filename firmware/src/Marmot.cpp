#include "Marmot.h"

#include <Gfx.h>

#include "MarmotArt.h"

namespace ui {

void marmot(mrm::Panel& o, int16_t cx, int16_t top, uint32_t elapsedMs) {
    const mrm::Frame& pose = mrm::frameAt(art::kMarmotBob, elapsedMs);
    const int16_t burrowX = cx - art::kBurrowHalfWidth;
    const int16_t burrowY = top + art::kBurrowTopRow;
    const int16_t marmotX = cx - art::kMarmot.w / 2 + pose.dx;

    o.setColor(WHITE);
    mrm::gfx::drawIcon(o, burrowX, burrowY, art::kBurrowBack);
    o.setColor(BLACK);
    o.fillRect(marmotX + art::kMarmotBodyLeft, burrowY, art::kMarmotBodyWidth, art::kBurrowBack.h);

    o.setColor(WHITE);
    mrm::gfx::drawIcon(o, marmotX, top + pose.dy, *pose.icon);
    o.setColor(BLACK);
    mrm::gfx::drawIcon(o, burrowX, burrowY, art::kBurrowCut);

    o.setColor(WHITE);
    mrm::gfx::drawIcon(o, burrowX, burrowY + art::kBurrowBack.h, art::kBurrowFront);
}

} // namespace ui
