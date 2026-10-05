#undef NDEBUG
#include <cassert>
#include <cstring>

#include "LogRing.h"

using Ring = mrm::LogRing<4, 8>;

int main() {
    Ring ring;
    ring.clear();
    assert(ring.recover());
    assert(ring.count == 0);

    ring.push("um");
    ring.push("dois");
    assert(ring.count == 2);
    assert(strcmp(ring.at(0), "um") == 0);
    assert(strcmp(ring.at(1), "dois") == 0);

    ring.push("tres");
    ring.push("quatro");
    ring.push("cinco");
    assert(ring.count == 4);
    assert(strcmp(ring.at(0), "dois") == 0);
    assert(strcmp(ring.at(3), "cinco") == 0);

    ring.push("123456789");
    assert(strcmp(ring.at(3), "1234567") == 0);

    ring.push("a\r\nb");
    assert(strcmp(ring.at(3), "ab") == 0);

    const uint8_t before = ring.count;
    ring.push("");
    ring.push("\r\n");
    assert(ring.count == before);
    assert(strcmp(ring.at(3), "ab") == 0);

    Ring kept = ring;
    memset(kept.lines[0], 'x', sizeof(kept.lines[0]));
    assert(kept.recover());
    assert(strlen(kept.lines[0]) == 7);

    Ring dirty = ring;
    dirty.lines[0][0] = '\x01';
    dirty.lines[0][1] = char(0xC3);
    dirty.lines[0][2] = '\0';
    assert(dirty.recover());
    assert(strcmp(dirty.lines[0], "??") == 0);

    Ring garbage;
    memset(&garbage, 0xAB, sizeof(garbage));
    assert(!garbage.recover());
    assert(garbage.count == 0);
    assert(garbage.magic == Ring::kMagic);

    Ring badHead = ring;
    badHead.head = 4;
    assert(!badHead.recover());

    Ring badCount = ring;
    badCount.count = 5;
    assert(!badCount.recover());

    return 0;
}
