#pragma once

#include <JsonOut.h>

// Respostas de leitura do portal (/api/state, /api/tracks, /api/loose) escritas em pedaços.
namespace portaljson {

struct State {
    const char* lang;
    const char* name;
    uint64_t freeBytes;
    uint64_t totalBytes;
    uint32_t reserveBytes;
    uint32_t maxPlaylists;
    uint32_t maxTracks;
    uint32_t maxPlaylistLen;
    uint32_t maxTrackLen;
    uint32_t maxFileBytes;
};

inline void next(mrm::JsonOut& json, const char* name) {
    json.raw(",");
    json.key(name);
}

inline void numField(mrm::JsonOut& json, const char* name, uint64_t value) {
    next(json, name);
    json.num(value);
}

inline void strField(mrm::JsonOut& json, const char* name, const char* value) {
    next(json, name);
    json.str(value);
}

// Abre o objeto e a lista "pl". Os itens vêm de playlistItem e endState fecha.
inline void beginState(mrm::JsonOut& json, const State& state) {
    json.raw("{");
    json.key("lang");
    json.str(state.lang);

    strField(json, "name", state.name);
    numField(json, "free", state.freeBytes);
    numField(json, "total", state.totalBytes);
    numField(json, "reserve", state.reserveBytes);

    next(json, "lim");
    json.raw("{");
    json.key("pl");
    json.num(state.maxPlaylists);

    numField(json, "tr", state.maxTracks);
    numField(json, "plLen", state.maxPlaylistLen);
    numField(json, "trLen", state.maxTrackLen);
    numField(json, "file", state.maxFileBytes);

    json.raw("}");
    next(json, "pl");
    json.raw("[");
}

inline void endState(mrm::JsonOut& json, const char* last, uint32_t lastToken) {
    json.raw("]");
    strField(json, "last", last);
    numField(json, "lu", lastToken);
    json.raw("}");
}

inline void openItem(mrm::JsonOut& json, bool first, const char* name) {
    if (!first)
        json.raw(",");
    json.raw("{");
    json.key("n");
    json.str(name);
}

inline void playlistItem(mrm::JsonOut& json, bool first, const char* name, uint32_t tracks, uint32_t bytes) {
    openItem(json, first, name);
    numField(json, "c", tracks);
    numField(json, "b", bytes);
    json.raw("}");
}

inline void trackItem(mrm::JsonOut& json, bool first, const char* name, uint32_t bytes) {
    openItem(json, first, name);
    numField(json, "b", bytes);
    json.raw("}");
}

inline void looseItem(mrm::JsonOut& json, bool first, const char* name, uint32_t bytes, const char* source) {
    openItem(json, first, name);
    numField(json, "b", bytes);
    strField(json, "s", source);
    json.raw("}");
}

inline void beginTracks(mrm::JsonOut& json, const char* playlist) {
    json.raw("{");
    json.key("p");
    json.str(playlist);
    next(json, "t");
    json.raw("[");
}

inline void endTracks(mrm::JsonOut& json) {
    json.raw("]}");
}

inline void beginLoose(mrm::JsonOut& json) {
    json.raw("{");
    json.key("t");
    json.raw("[");
}

inline void endLoose(mrm::JsonOut& json, bool more) {
    json.raw("]");
    numField(json, "more", more ? 1 : 0);
    json.raw("}");
}

} // namespace portaljson
