#undef NDEBUG
#include <cassert>
#include <cstdio>

#include <PathRules.h>

#include "PortalQuery.h"

using portal::Param;

static void testTextOk() {
    char value[16];
    assert(portal::queryText("p=Rock&t=x", "p", value, sizeof value) == Param::Ok);
    assert(strcmp(value, "Rock") == 0);
    assert(portal::queryText("p=Rock&t=x", "t", value, sizeof value) == Param::Ok);
    assert(strcmp(value, "x") == 0);
    assert(portal::queryText("p=A%20B%C3%A9", "p", value, sizeof value) == Param::Ok);
    assert(strcmp(value, "A B\xC3\xA9") == 0);
    assert(portal::queryText("p=a+b", "p", value, sizeof value) == Param::Ok);
    assert(strcmp(value, "a+b") == 0);
}

static void testTextMissing() {
    char value[16];
    assert(portal::queryText("", "p", value, sizeof value) == Param::Missing);
    assert(portal::queryText("pp=1&q=2", "p", value, sizeof value) == Param::Missing);
}

static void testTextTruncated() {
    char value[4];
    assert(portal::queryText("p=abc", "p", value, sizeof value) == Param::Ok);
    assert(portal::queryText("p=abcd", "p", value, sizeof value) == Param::Bad);
}

static void testTextInvalid() {
    char value[16];
    assert(portal::queryText("p=%", "p", value, sizeof value) == Param::Bad);
    assert(portal::queryText("p=%4", "p", value, sizeof value) == Param::Bad);
    assert(portal::queryText("p=%zz", "p", value, sizeof value) == Param::Bad);
    assert(portal::queryText("p=a%00b", "p", value, sizeof value) == Param::Bad);
}

static void testUint() {
    uint32_t value = 0;
    assert(portal::queryUint("n=42&x=1", "n", value) == Param::Ok);
    assert(value == 42);
    assert(portal::queryUint("n=4294967295", "n", value) == Param::Ok);
    assert(value == UINT32_MAX);
    assert(portal::queryUint("x=1", "n", value) == Param::Missing);
    assert(portal::queryUint("n=4294967296", "n", value) == Param::Bad);
    assert(portal::queryUint("n=", "n", value) == Param::Bad);
    assert(portal::queryUint("n=-1", "n", value) == Param::Bad);
    assert(portal::queryUint("n=12a", "n", value) == Param::Bad);
}

static void testName() {
    char value[8];
    assert(portal::queryName("p=Rock", "p", value, sizeof value, mrm::validPlaylistName));
    assert(!portal::queryName("x=1", "p", value, sizeof value, mrm::validPlaylistName));
    assert(!portal::queryName("p=abcdefgh", "p", value, sizeof value, mrm::validPlaylistName));
    assert(!portal::queryName("p=a%2Fb", "p", value, sizeof value, mrm::validPlaylistName));
    assert(!portal::queryName("p=.x", "p", value, sizeof value, mrm::validPlaylistName));
}

int main() {
    testTextOk();
    testTextMissing();
    testTextTruncated();
    testTextInvalid();
    testUint();
    testName();
    puts("test_portal_query ok");
}
