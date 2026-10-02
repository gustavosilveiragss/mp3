#include "Library.h"

namespace {

template <typename Dir, typename Valid, typename Insert>
void forEachEntry(Dir& dir, Valid valid, Insert insert) {
    for (File e = dir.openNextFile(); e; e = dir.openNextFile()) {
        if (valid(e))
            insert(e.name());
    }
}

// Insercao ordenada sem alocar: as listas sao pequenas e ficam em arrays fixos.
template <size_t N>
bool insertSorted(char (*list)[N], uint16_t& count, uint16_t capacity, const char* name) {
    if (count >= capacity)
        return false;
    uint16_t at = count;
    while (at > 0 && strcasecmp(list[at - 1], name) > 0)
        --at;
    memmove(list[at + 1], list[at], (count - at) * N);
    strlcpy(list[at], name, N);
    ++count;
    return true;
}

struct PlaylistDir {
    char path[sizeof(kMusicDir) + mrm::kMaxPlaylistLen + 2];
    explicit PlaylistDir(const char* playlist) { snprintf(path, sizeof(path), "%s/%s", kMusicDir, playlist); }
};

bool isTrack(File& e) {
    return !e.isDirectory() && mrm::validTrackName(e.name());
}

} // namespace

void Library::scan() {
    playlists_ = 0;
    trackCount_ = 0;
    selected_ = kNone;
    truncated_ = false;

    File root = fs_.open(kMusicDir);
    if (!root || !root.isDirectory())
        return;

    uint16_t count = 0;
    forEachEntry(
        root, [](File& e) { return e.isDirectory() && mrm::validPlaylistName(e.name()); },
        [&](const char* name) { truncated_ |= !insertSorted(names_, count, kMaxPlaylists, name); });
    playlists_ = static_cast<uint8_t>(count);

    for (uint8_t p = 0; p < playlists_; ++p)
        counts_[p] = countTracks(names_[p]);
}

uint16_t Library::countTracks(const char* playlist) {
    File dir = fs_.open(PlaylistDir(playlist).path);
    uint16_t count = 0;
    if (dir && dir.isDirectory())
        forEachEntry(dir, isTrack, [&](const char*) { ++count; });
    if (count > kMaxTracks) {
        truncated_ = true;
        count = kMaxTracks;
    }
    return count;
}

bool Library::select(uint8_t playlist) {
    if (playlist >= playlists_)
        return false;
    selected_ = playlist;
    trackCount_ = 0;

    File dir = fs_.open(PlaylistDir(names_[playlist]).path);
    if (!dir || !dir.isDirectory())
        return false;
    forEachEntry(dir, isTrack, [&](const char* name) { insertSorted(tracks_, trackCount_, kMaxTracks, name); });
    return true;
}

bool Library::trackPath(uint16_t track, char* out, size_t size) const {
    if (selected_ == kNone || track >= trackCount_)
        return false;
    const int n = snprintf(out, size, "%s/%s/%s", kMusicDir, names_[selected_], tracks_[track]);
    return n > 0 && static_cast<size_t>(n) < size;
}
