#undef NDEBUG
#include <cassert>

#include "Bitmap.h"

int main() {
    static constexpr auto kSmall = mrm::bitmap<10, 2>(R"(
        #.......##
        .#.#....#.
    )");
    static constexpr mrm::gfx::Icon icon = kSmall.icon();
    static_assert(sizeof(kSmall.bytes) == 4);

    const uint8_t expected[] = {0x01, 0x03, 0x0a, 0x01};
    assert(icon.w == 10 && icon.h == 2);
    for (int i = 0; i < 4; ++i)
        assert(icon.xbm[i] == expected[i]);
    return 0;
}
