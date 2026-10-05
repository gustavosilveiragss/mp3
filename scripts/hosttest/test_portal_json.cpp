#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <string>

#include "PortalJson.h"

// Respostas montadas como antes (snprintf + raw) para comparar byte a byte com o JsonOut.
static std::string legacyState(const char* lang, const char* name, const portaljson::State& s, const char* list,
                               const char* last, uint32_t lu) {
    char buf[160];
    std::string out = std::string("{\"lang\":\"") + lang + "\",\"name\":\"" + name + "\"";
    snprintf(buf, sizeof(buf), ",\"free\":%llu,\"total\":%llu,\"reserve\":%lu,",
             static_cast<unsigned long long>(s.freeBytes), static_cast<unsigned long long>(s.totalBytes),
             static_cast<unsigned long>(s.reserveBytes));
    out += buf;
    snprintf(buf, sizeof(buf), "\"lim\":{\"pl\":%u,\"tr\":%u,\"plLen\":%u,\"trLen\":%u,\"file\":%lu},\"pl\":[",
             unsigned(s.maxPlaylists), unsigned(s.maxTracks), unsigned(s.maxPlaylistLen), unsigned(s.maxTrackLen),
             static_cast<unsigned long>(s.maxFileBytes));
    out += buf;
    out += list;
    snprintf(buf, sizeof(buf), "],\"last\":\"%s\",\"lu\":%lu}", last, static_cast<unsigned long>(lu));
    return out + buf;
}

static void testState() {
    const portaljson::State state{"pt", "Marmota \"1\"", 31914983424ULL, 31914983424ULL, 65536, 12, 99, 48, 48,
                                  1UL << 30};
    WebServer server;
    mrm::JsonOut json(server);
    portaljson::beginState(json, state);
    portaljson::playlistItem(json, true, "Rock", 3, 12345678);
    portaljson::playlistItem(json, false, "Jazz\\Blues", 0, 0);
    portaljson::endState(json, "aborted", 4294967295u);
    json.end();
    const char* list = "{\"n\":\"Rock\",\"c\":3,\"b\":12345678},{\"n\":\"Jazz\\\\Blues\",\"c\":0,\"b\":0}";
    assert(server.body == legacyState("pt", "Marmota \\\"1\\\"", state, list, "aborted", 4294967295u));
}

static void testTracksAndLoose() {
    WebServer tracks;
    mrm::JsonOut json(tracks);
    portaljson::beginTracks(json, "Rock");
    portaljson::trackItem(json, true, "01 - a.mp3", 4000000000u);
    portaljson::trackItem(json, false, "02 - \"b\".mp3", 7);
    portaljson::endTracks(json);
    json.end();
    assert(tracks.body == "{\"p\":\"Rock\",\"t\":[{\"n\":\"01 - a.mp3\",\"b\":4000000000},"
                          "{\"n\":\"02 - \\\"b\\\".mp3\",\"b\":7}]}");

    WebServer loose;
    mrm::JsonOut looseJson(loose);
    portaljson::beginLoose(looseJson);
    portaljson::looseItem(looseJson, true, "x.mp3", 1, "music");
    portaljson::looseItem(looseJson, false, "y.mp3", 2, "root");
    portaljson::endLoose(looseJson, true);
    looseJson.end();
    assert(loose.body == "{\"t\":[{\"n\":\"x.mp3\",\"b\":1,\"s\":\"music\"},{\"n\":\"y.mp3\",\"b\":2,\"s\":\"root\"}],\"more\":1}");

    WebServer empty;
    mrm::JsonOut emptyJson(empty);
    portaljson::beginLoose(emptyJson);
    portaljson::endLoose(emptyJson, false);
    emptyJson.end();
    assert(empty.body == "{\"t\":[],\"more\":0}");
}

int main() {
    testState();
    testTracksAndLoose();
    puts("test_portal_json ok");
}
