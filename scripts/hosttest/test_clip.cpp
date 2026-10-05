#undef NDEBUG
#include <cassert>

#include "Clip.h"

static const mrm::Frame kFrames[] = {{nullptr, 1, 1, 300}, {nullptr, 0, 0, 200}, {nullptr, -1, -1, 500}};
static const mrm::Clip kClip{kFrames, 3};

int main() {
    assert(mrm::clipDurationMs(kClip) == 1000);
    assert(mrm::frameAt(kClip, 0).dx == 1);
    assert(mrm::frameAt(kClip, 299).dx == 1);
    assert(mrm::frameAt(kClip, 300).dx == 0);
    assert(mrm::frameAt(kClip, 499).dx == 0);
    assert(mrm::frameAt(kClip, 500).dx == -1);
    assert(mrm::frameAt(kClip, 999).dx == -1);
    assert(mrm::frameAt(kClip, 1000).dx == 1);
    assert(mrm::frameAt(kClip, 3 * 1000 + 350).dx == 0);
    return 0;
}
