#include "Library.h"

namespace {

void playlistDir(const char* playlist, char* out, size_t size) {
    snprintf(out, size, "%s/%.*s", kMusicDir, int(mrm::kMaxPlaylistLen), playlist);
}

} // namespace

void Library::scan() {
    selected_[0] = '\0';
    playlists_.open(kMusicDir);
    tracks_.open("");
}

bool Library::playlistIndex(const char* name, uint8_t& index) const {
    uint16_t at = 0;
    const bool found = playlists_.indexOf(name, at) && at < kMaxPlaylists;
    if (found)
        index = static_cast<uint8_t>(at);
    return found;
}

uint16_t Library::trackCount(uint8_t playlist) const {
    uint16_t& cached = playlists_.aux(playlist);
    if (cached == DirWindow::kUnknown) {
        char dir[sizeof(kMusicDir) + mrm::kMaxPlaylistLen + 2];
        playlistDir(playlists_.name(playlist), dir, sizeof(dir));
        cached = min<uint16_t>(dirlist::count(dir, false), kMaxTracks);
    }
    return cached;
}

uint32_t Library::totalTracks() const {
    char dir[sizeof(kMusicDir) + mrm::kMaxPlaylistLen + 2];
    uint32_t sum = 0;
    dirlist::each(kMusicDir, [&](const dirlist::Entry& e) {
        if (dirlist::valid(e, true)) {
            playlistDir(e.name, dir, sizeof(dir));
            sum += min<uint16_t>(dirlist::count(dir, false), kMaxTracks);
        }
        return true;
    });
    return sum;
}

bool Library::select(uint8_t playlist) {
    if (playlist >= playlistCount())
        return false;
    strlcpy(selected_, playlists_.name(playlist), sizeof(selected_));
    char dir[sizeof(kMusicDir) + mrm::kMaxPlaylistLen + 2];
    playlistDir(selected_, dir, sizeof(dir));
    tracks_.open(dir);
    return true;
}

bool Library::trackPath(const char* track, char* out, size_t size) const {
    if (!selected_[0] || !track[0])
        return false;
    const int n = snprintf(out, size, "%s/%s/%s", kMusicDir, selected_, track);
    return n > 0 && static_cast<size_t>(n) < size;
}
