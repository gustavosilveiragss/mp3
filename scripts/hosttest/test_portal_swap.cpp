#undef NDEBUG
#include <cassert>
#include <cstdio>

#include "PortalPaths.h"
#include "PortalSwap.h"

static trackswap::SwapNames names(const char* first, const char* second) {
    trackswap::SwapNames result{};
    result.playlist = "Rock";
    result.first = first;
    result.second = second;
    return result;
}

static void testNumPrefix() {
    assert(trackswap::numPrefix("01 - x.mp3") == 5);
    assert(trackswap::numPrefix("123 - x.mp3") == 6);
    assert(trackswap::numPrefix("x.mp3") == 0);
    assert(trackswap::numPrefix("01-x.mp3") == 0);
    assert(trackswap::numPrefix(" - x.mp3") == 0);
}

static void testCanSwap() {
    assert(trackswap::prefixesCanSwap("01 - a.mp3", "02 - b.mp3"));
    assert(trackswap::prefixesCanSwap("1 - a.mp3", "10 - b.mp3"));
    assert(!trackswap::prefixesCanSwap("01 - a.mp3", "01 - b.mp3"));
    assert(!trackswap::prefixesCanSwap("a.mp3", "02 - b.mp3"));
    assert(!trackswap::prefixesCanSwap("01 - a.mp3", "b.mp3"));
}

static void testBuild() {
    trackswap::SwapNames pair = names("01 - x.mp3", "02 - y.mp3");
    assert(trackswap::buildSwappedNames(pair));
    assert(strcmp(pair.newFirst, "02 - x.mp3") == 0);
    assert(strcmp(pair.newSecond, "01 - y.mp3") == 0);

    trackswap::SwapNames different = names("1 - x.mp3", "10 - y.mp3");
    assert(trackswap::buildSwappedNames(different));
    assert(strcmp(different.newFirst, "10 - x.mp3") == 0);
    assert(strcmp(different.newSecond, "1 - y.mp3") == 0);
}

static void testBuildTruncated() {
    const char* longFirst = "1 - aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.mp3"; // 48 caracteres, cabe
    assert(strlen(longFirst) == mrm::kMaxTrackLen);
    trackswap::SwapNames tooLong = names(longFirst, "100 - y.mp3");
    assert(!trackswap::buildSwappedNames(tooLong));
}

static void testPaths() {
    char path[16];
    assert(portal::playlistDir(path, sizeof path, "/music", "Rock"));
    assert(strcmp(path, "/music/Rock") == 0);
    assert(portal::trackPath(path, sizeof path, "/music", "R", "a.mp3"));
    assert(strcmp(path, "/music/R/a.mp3") == 0);
    assert(portal::hiddenTrackPath(path, sizeof path, "/music", "R", "a.mp3"));
    assert(strcmp(path, "/music/R/.a.mp3") == 0);
    assert(portal::joinPath(path, sizeof path, "", "a.mp3"));
    assert(strcmp(path, "/a.mp3") == 0);
    assert(!portal::playlistDir(path, sizeof path, "/music", "0123456789"));
    assert(!portal::trackPath(path, sizeof path, "/music", "Rock", "abcdef.mp3"));
}

int main() {
    testNumPrefix();
    testCanSwap();
    testBuild();
    testBuildTruncated();
    testPaths();
    puts("test_portal_swap ok");
}
