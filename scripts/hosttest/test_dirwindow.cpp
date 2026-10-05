#undef NDEBUG
#include <cassert>
#include <string>
#include <unistd.h>
#include <sys/stat.h>

#include "DirScan.cpp"
#include "DirWindow.cpp"

static std::string gRoot;
namespace media {
const char* mountPoint() { return gRoot.c_str(); }
} // namespace media

static void touch(const std::string& path) {
    FILE* f = fopen(path.c_str(), "w");
    assert(f);
    fputs("x", f);
    fclose(f);
}

static void makeTree(int tracks) {
    char tmpl[] = "/tmp/dirwinXXXXXX";
    assert(mkdtemp(tmpl));
    gRoot = tmpl;
    mkdir((gRoot + "/music").c_str(), 0755);
    for (int i = 0; i < tracks; ++i) {
        char name[32];
        snprintf(name, sizeof(name), "%02d - t.mp3", i);
        touch(gRoot + "/music/" + name);
    }
    touch(gRoot + "/music/readme.txt"); // invalido
    mkdir((gRoot + "/music/pasta").c_str(), 0755); // diretorio nao e faixa
}

static std::string trackName(int i) {
    char name[32];
    snprintf(name, sizeof(name), "%02d - t.mp3", i);
    return name;
}

static void testChunk() {
    char names[4][dirlist::kName];
    uint32_t sizes[4];
    dirlist::Out out{names, sizes, 4};
    uint8_t n = dirlist::chunk({"/music", false, "", true}, out);
    assert(n == 4 && trackName(0) == names[0] && trackName(3) == names[3] && sizes[0] == 1);
    n = dirlist::chunk({"/music", false, trackName(5).c_str(), true}, out);
    assert(n == 4 && trackName(6) == names[0] && trackName(9) == names[3]);
    n = dirlist::chunk({"/music", false, "", false}, out);
    assert(n == 4 && trackName(16) == names[0] && trackName(19) == names[3]);
    n = dirlist::chunk({"/music", false, trackName(5).c_str(), false}, out);
    assert(n == 4 && trackName(1) == names[0] && trackName(4) == names[3]);
    n = dirlist::chunk({"/music", false, trackName(2).c_str(), false}, out);
    assert(n == 2 && trackName(0) == names[0] && trackName(1) == names[1]);
    n = dirlist::chunk({"/music", false, trackName(18).c_str(), true}, out);
    assert(n == 1 && trackName(19) == names[0]);
    dirlist::Out noSizes{names, nullptr, 4};
    n = dirlist::chunk({"/music", true, "", true}, noSizes); // playlists: so "pasta"
    assert(n == 1 && std::string("pasta") == names[0]);
}

static void testOrder() {
    assert(dirlist::order("a.mp3", "B.mp3") < 0);
    assert(dirlist::order("A.mp3", "a.mp3") < 0); // desempate pelos bytes
}

static void testWindow() {
    DirWindow w(false);
    w.open("/music");
    assert(w.total() == 20);
    assert(trackName(0) == w.name(0));
    assert(trackName(7) == w.name(7));
    assert(trackName(8) == w.name(8)); // anda um
    w.aux(8) = 42;
    assert(trackName(9) == w.name(9));
    assert(w.aux(8) == 42); // contagem mantida na janela que desliza
    assert(trackName(19) == w.name(19)); // salto longo, do fim
    assert(trackName(12) == w.name(12));
    assert(trackName(11) == w.name(11)); // para tras, ancora na janela
    assert(trackName(3) == w.name(3));
    assert(trackName(10) == w.name(10)); // salto do comeco, anda ate achar
    assert(std::string("") == w.name(20));
    assert(w.aux(20) == DirWindow::kUnknown);
    uint16_t index = 0;
    assert(w.indexOf(trackName(13).c_str(), index) && index == 13);
    assert(!w.indexOf("zzz.mp3", index) && index == 20);
    DirWindow empty(false);
    empty.open("");
    assert(empty.total() == 0 && std::string("") == empty.name(0));
}

int main() {
    makeTree(20);
    testOrder();
    testChunk();
    testWindow();
    std::string cmd = "rm -rf " + gRoot;
    return system(cmd.c_str());
}
