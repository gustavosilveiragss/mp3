#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <string>

#include "PathRules.h"

static void testPlaylistNames() {
    assert(mrm::validPlaylistName("Rock"));
    assert(mrm::validPlaylistName("Viagem longa"));
    assert(mrm::validPlaylistName("Cora\xC3\xA7\xC3\xA3o"));
    assert(!mrm::validPlaylistName(nullptr));
    assert(!mrm::validPlaylistName(""));
    assert(!mrm::validPlaylistName(".."));
    assert(!mrm::validPlaylistName("."));
    assert(!mrm::validPlaylistName("a/b"));
    assert(!mrm::validPlaylistName("a\\b"));
    assert(!mrm::validPlaylistName(" Rock"));
    assert(!mrm::validPlaylistName("Rock "));
    assert(!mrm::validPlaylistName("Rock."));
    assert(!mrm::validPlaylistName("a*b"));
    assert(!mrm::validPlaylistName("a\tb"));
}

static void testPlaylistLength() {
    assert(mrm::validPlaylistName(std::string(mrm::kMaxPlaylistLen, 'a').c_str()));
    assert(!mrm::validPlaylistName(std::string(mrm::kMaxPlaylistLen + 1, 'a').c_str()));
}

static void testTrackNames() {
    assert(mrm::validTrackName("01 - Nome.mp3"));
    assert(mrm::validTrackName("x.MP3"));
    assert(!mrm::validTrackName(nullptr));
    assert(!mrm::validTrackName(""));
    assert(!mrm::validTrackName(".mp3"));
    assert(!mrm::validTrackName("faixa.wav"));
    assert(!mrm::validTrackName("faixa"));
    assert(!mrm::validTrackName("../x.mp3"));
    assert(!mrm::validTrackName("a/b.mp3"));
}

static void testTrackLength() {
    const std::string longest = std::string(mrm::kMaxTrackLen - 4, 'a') + ".mp3";
    assert(mrm::validTrackName(longest.c_str()));
    assert(!mrm::validTrackName((std::string("a") + longest).c_str()));
}

int main() {
    testPlaylistNames();
    testPlaylistLength();
    testTrackNames();
    testTrackLength();
    puts("test_path_rules ok");
}
