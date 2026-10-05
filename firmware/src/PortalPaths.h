#pragma once

#include <cstddef>
#include <cstdio>

// Caminhos no cartão montados uma vez, com checagem de truncamento (false: não coube em `cap`). Funções puras.
namespace portal {

inline bool fits(int written, size_t cap) {
    return written >= 0 && size_t(written) < cap;
}

inline bool joinPath(char* out, size_t cap, const char* root, const char* name) {
    return fits(snprintf(out, cap, "%s/%s", root, name), cap);
}

inline bool playlistDir(char* out, size_t cap, const char* root, const char* playlist) {
    return joinPath(out, cap, root, playlist);
}

inline bool trackPath(char* out, size_t cap, const char* root, const char* playlist, const char* track) {
    return fits(snprintf(out, cap, "%s/%s/%s", root, playlist, track), cap);
}

// Nome oculto (".faixa.mp3") usado como parada intermediária da troca, que recoverSwap procura no boot.
inline bool hiddenTrackPath(char* out, size_t cap, const char* root, const char* playlist, const char* track) {
    return fits(snprintf(out, cap, "%s/%s/.%s", root, playlist, track), cap);
}

} // namespace portal
