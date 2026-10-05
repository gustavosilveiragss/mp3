#pragma once

#include <marmota.h>

namespace art {

// Origem do desenho: canto superior esquerdo do sprite.
inline constexpr auto kPlayBitmap = mrm::bitmap<10, 12>(R"(
    #.........
    ##........
    ####......
    #####.....
    #######...
    ########..
    ##########
    #########.
    #######...
    #####.....
    ###.......
    #.........
)");
inline constexpr mrm::gfx::Icon kPlay = kPlayBitmap.icon();

inline constexpr auto kPauseBitmap = mrm::bitmap<9, 12>(R"(
    ###...###
    ###...###
    ###...###
    ###...###
    ###...###
    ###...###
    ###...###
    ###...###
    ###...###
    ###...###
    ###...###
    ###...###
)");
inline constexpr mrm::gfx::Icon kPause = kPauseBitmap.icon();

inline constexpr auto kSpeakerWaveBitmap = mrm::bitmap<14, 14>(R"(
    ........#.....
    .......####...
    ......###..#..
    .....####...#.
    #########....#
    #########....#
    ###.#####....#
    ###.#####....#
    ###..####....#
    .....####...#.
    ......###..#..
    ......#####...
    .......##.....
    ........#.....
)");
inline constexpr mrm::gfx::Icon kSpeakerWave = kSpeakerWaveBitmap.icon();

inline constexpr auto kSpeakerMuteBitmap = mrm::bitmap<18, 14>(R"(
    ........#.........
    .......##.........
    ......###.........
    .....####..#.....#
    #########...#...#.
    #########....#.#..
    ###.#####.....#...
    ###.#####.....#...
    ###..####....#.#..
    .....####...#...#.
    ......###..#.....#
    ......###.........
    .......##.........
    ........#.........
)");
inline constexpr mrm::gfx::Icon kSpeakerMute = kSpeakerMuteBitmap.icon();

inline constexpr auto kLockClosedBitmap = mrm::bitmap<7, 10>(R"(
    .#####.
    .#...#.
    .#...#.
    .#...#.
    #######
    #######
    #######
    #######
    #######
    #######
)");
inline constexpr mrm::gfx::Icon kLockClosed = kLockClosedBitmap.icon();

inline constexpr auto kLockOpenBitmap = mrm::bitmap<7, 10>(R"(
    .......
    .####..
    .#..#..
    .#.....
    #######
    #######
    #######
    #######
    #######
    #######
)");
inline constexpr mrm::gfx::Icon kLockOpen = kLockOpenBitmap.icon();

} // namespace art
