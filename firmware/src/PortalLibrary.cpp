#include "Library.h"
#include "Media.h"
#include "Portal.h"
#include "PortalJson.h"
#include "PortalPaths.h"
#include "PortalQuery.h"

using namespace portal;
using namespace portalhttp;

namespace {

constexpr char kSrcMusic[] = "music";
constexpr char kSrcRoot[] = "root";

} // namespace

void Portal::beginJsonStream() {
    server_->sendHeader("Cache-Control", "no-store");
    server_->setContentLength(CONTENT_LENGTH_UNKNOWN);
    server_->send(kOk, kJsonType, "");
}

void Portal::sendState() {
    touch();
    beginJsonStream();

    mrm::JsonOut json(*server_);
    portaljson::beginState(json, {mrm::Locale::code(locale_.lang()), ssid_, media::freeBytes(), media::totalBytes(),
                                  kReserveBytes, Library::kMaxPlaylists, Library::kMaxTracks, mrm::kMaxPlaylistLen,
                                  mrm::kMaxTrackLen, kMaxFileBytes});

    writePlaylists(json);
    portaljson::endState(json, last_, lastU_);
    json.end();
}

void Portal::writePlaylists(mrm::JsonOut& json) {
    char after[kTrackBuf] = "";
    char path[kPathMax];
    uint8_t count = 0;
    bool first = true;

    while (nextChunk(kMusicDir, true, after, count)) {
        for (uint8_t i = 0; i < count; ++i) {
            uint32_t bytes = 0;
            if (!playlistDir(path, sizeof(path), kMusicDir, chunk_[i]))
                continue;

            const uint16_t tracks = dirlist::count(path, false, &bytes);
            portaljson::playlistItem(json, first, chunk_[i], tracks, bytes);
            first = false;
        }
    }
}

void Portal::sendTracks() {
    touch();

    char playlist[kPlaylistBuf], dir[kPathMax];
    if (server_->arg("p").length() >= sizeof(playlist))
        return sendError(kBadRequest, kErrName);

    strlcpy(playlist, server_->arg("p").c_str(), sizeof(playlist));
    if (!mrm::validPlaylistName(playlist) || !playlistDir(dir, sizeof(dir), kMusicDir, playlist) ||
        !dirlist::exists(dir))
        return sendError(kNotFound, kErrName);

    beginJsonStream();
    mrm::JsonOut json(*server_);

    char after[kTrackBuf] = "";
    uint8_t count = 0;
    bool first = true;
    portaljson::beginTracks(json, playlist);

    while (nextChunk(dir, false, after, count)) {
        for (uint8_t i = 0; i < count; ++i) {
            portaljson::trackItem(json, first, chunk_[i], chunkSize_[i]);
            first = false;
        }
    }

    portaljson::endTracks(json);
    json.end();
}

// .mp3 soltos: os de /music e os da raiz da mídia, até kMaxLoose, cada origem em ordem de nome.
void Portal::sendLoose() {
    touch();

    beginJsonStream();
    mrm::JsonOut json(*server_);

    uint8_t total = 0;
    bool more = false;
    portaljson::beginLoose(json);

    for (const bool root : {false, true}) {
        char after[kTrackBuf] = "";
        uint8_t count = 0;

        while (!more && nextChunk(root ? "/" : kMusicDir, false, after, count)) {
            for (uint8_t i = 0; i < count && !more; ++i) {
                more = total >= kMaxLoose;
                if (more)
                    break;

                portaljson::looseItem(json, total == 0, chunk_[i], chunkSize_[i], root ? kSrcRoot : kSrcMusic);
                ++total;
            }
        }
    }

    portaljson::endLoose(json, more);
    json.end();
}

// "music" (padrão) ou "root": pasta dos .mp3 soltos, sem a barra final.
bool Portal::looseBase(char* base, size_t cap) {
    char src[kLooseSrcMax];
    switch (queryText(query_.text, "src", src, sizeof(src))) {
    case Param::Missing:
        strlcpy(base, kMusicDir, cap);
        return true;

    case Param::Ok:
        if (strcmp(src, kSrcMusic) == 0)
            strlcpy(base, kMusicDir, cap);
        else if (strcmp(src, kSrcRoot) == 0)
            base[0] = '\0';
        else
            return false;

        return true;
    default:
        return false;
    }
}

// p (e n opcional) do X-Q como caminho dentro de /music. P vazio com n é um .mp3 solto (src).
// Devolve 0, ou o código HTTP do erro.
int Portal::resolve(char* path, size_t cap, bool& track) {
    if (queryText(query_.text, "p", query_.playlist, sizeof(query_.playlist)) != Param::Ok)
        return kBadRequest;

    const Param n = queryText(query_.text, "n", query_.track, sizeof(query_.track));
    track = n == Param::Ok;
    if (n == Param::Bad || (track && !mrm::validTrackName(query_.track)))
        return kBadRequest;

    if (!query_.playlist[0]) {
        char base[sizeof(kMusicDir)];
        if (!track || !looseBase(base, sizeof(base)) || !joinPath(path, cap, base, query_.track))
            return kBadRequest;

        return fs_.exists(path) ? 0 : kNotFound;
    }

    const bool built = track ? trackPath(path, cap, kMusicDir, query_.playlist, query_.track)
                             : playlistDir(path, cap, kMusicDir, query_.playlist);
    if (!mrm::validPlaylistName(query_.playlist) || !built)
        return kBadRequest;

    return fs_.exists(path) ? 0 : kNotFound;
}

void Portal::doDelete() {
    if (!claim())
        return;

    char path[kPathMax];
    bool track = false;
    const int code = resolve(path, sizeof(path), track);
    if (code)
        return sendError(code, kErrName);

    changed_ = true; // mesmo que o apagar falhe no meio, o Player precisa reler
    if (!(track ? fs_.remove(path) : purge(path)))
        return sendError(kServerError, kErrIo);

    sendOk(true);
}

void Portal::doRename() {
    if (!claim())
        return;

    char src[kPathMax], dst[kPathMax];
    bool track = false;
    const int code = resolve(src, sizeof(src), track);
    if (code)
        return sendError(code, kErrName);

    const auto validate = track ? mrm::validTrackName : mrm::validPlaylistName;
    const bool named = queryName(query_.text, "to", query_.target, sizeof(query_.target), validate);
    const bool built = named && (track ? trackPath(dst, sizeof(dst), kMusicDir, query_.playlist, query_.target)
                                       : playlistDir(dst, sizeof(dst), kMusicDir, query_.target));
    if (!built)
        return sendError(kBadRequest, kErrName);

    const char* from = track ? query_.track : query_.playlist;
    if (fs_.exists(dst)) {
        const bool onlyCaseDiffers = track && strcasecmp(from, query_.target) == 0 && strcmp(from, query_.target) != 0;
        if (!onlyCaseDiffers)
            return sendError(kConflict, kErrExists);

        return renameCase(src, dst);
    }

    if (!fs_.rename(src, dst))
        return sendError(kServerError, kErrIo);

    changed_ = true;
    sendOk(false);
}

// Só maiúsculas/minúsculas de faixa: em FAT o destino "existe" porque e o próprio arquivo. Passa por
// ".nome" (que recoverSwap recupera após queda) e confere de novo. Se o destino ainda existe, era outro arquivo.
void Portal::renameCase(const char* src, const char* dst) {

    char tmp[kPathMax];
    const char* slash = strrchr(dst, '/');
    if (snprintf(tmp, sizeof(tmp), "%.*s.%s", int(slash - dst + 1), dst, slash + 1) >= int(sizeof(tmp)))
        return sendError(kBadRequest, kErrName);

    if (!fs_.rename(src, tmp))
        return sendError(kServerError, kErrIo);

    changed_ = true;
    const bool other = fs_.exists(dst);
    if (!other && fs_.rename(tmp, dst))
        return sendOk(false);

    if (!fs_.rename(tmp, src))
        return sendError(kServerError, kErrIo); // fica ".nome": recoverSwap devolve no próximo begin

    sendError(other ? kConflict : kServerError, other ? kErrExists : kErrIo);
}
