#include "Library.h"
#include "Media.h"
#include "Portal.h"
#include "PortalPaths.h"
#include "PortalQuery.h"
#include "PortalSwap.h"

using namespace portal;
using namespace portalhttp;

// Gerencia o que já estava na mídia: mover e reordenar.

bool Portal::targetFull(const char* dir, bool isNew) {
    return isNew ? dirlist::count(kMusicDir, true, nullptr) >= Library::kMaxPlaylists
                 : dirlist::count(dir, false, nullptr) >= Library::kMaxTracks;
}

// Move um .mp3 (solto ou de outra playlist) para /music/<to>/. Nunca sobrescreve. Só rename no mesmo
// FS, então não gasta espaço. Playlist de origem que fica vazia some.
void Portal::doMove() {
    if (!claim())
        return;

    char source[kPathMax], targetDir[kPathMax], targetPath[kPathMax];
    bool track = false;
    const int code = resolve(source, sizeof(source), track);
    if (code || !track)
        return sendError(code ? code : kBadRequest, kErrName);

    if (!portal::queryName(query_.text, "to", query_.target, sizeof(query_.target), mrm::validPlaylistName) ||
        !portal::playlistDir(targetDir, sizeof(targetDir), kMusicDir, query_.target) ||
        !portal::trackPath(targetPath, sizeof(targetPath), kMusicDir, query_.target, query_.track))
        return sendError(kBadRequest, kErrName);

    const bool isNew = !fs_.exists(targetDir);
    if (!isNew && fs_.exists(targetPath))
        return sendError(kConflict, kErrExists);

    if (targetFull(targetDir, isNew))
        return sendError(kConflict, kErrLimit);

    if (isNew && !fs_.mkdir(targetDir))
        return sendError(kServerError, kErrIo);

    changed_ = true;
    if (!fs_.rename(source, targetPath)) {
        if (isNew)
            fs_.rmdir(targetDir);
        return sendError(kServerError, kErrIo);
    }

    removeSourceIfEmpty();

    sendOk(false);
}

void Portal::removeSourceIfEmpty() {
    char sourceDir[kPathMax];
    if (!query_.playlist[0] || !portal::playlistDir(sourceDir, sizeof(sourceDir), kMusicDir, query_.playlist))
        return;

    if (dirlist::count(sourceDir, false, nullptr) == 0)
        fs_.rmdir(sourceDir); // falha sem barulho se sobrou algo que não e faixa
}

bool Portal::trackExists(const char* playlist, const char* track) {
    char path[kPathMax];
    return portal::trackPath(path, sizeof(path), kMusicDir, playlist, track) && fs_.exists(path);
}

bool Portal::readSwapQuery(trackswap::SwapNames& names) {

    names.playlist = query_.playlist;
    names.first = query_.track;
    names.second = query_.target;

    if (!queryName(query_.text, "p", query_.playlist, sizeof(query_.playlist), mrm::validPlaylistName))
        return false;

    if (!queryName(query_.text, "a", query_.track, sizeof(query_.track), mrm::validTrackName))
        return false;

    if (!queryName(query_.text, "b", query_.target, sizeof(query_.target), mrm::validTrackName))
        return false;

    return strcmp(names.first, names.second) != 0;
}

// Passa por ".<newFirst>" (nome final do primeiro, escondido da biblioteca): queda no meio deixa só o
// oculto, que recoverSwap devolve no boot.
bool Portal::swapFiles(const trackswap::SwapNames& names) {
    char firstPath[kPathMax], secondPath[kPathMax], hiddenPath[kPathMax];
    char newFirstPath[kPathMax], newSecondPath[kPathMax];
    if (!trackPath(firstPath, sizeof(firstPath), kMusicDir, names.playlist, names.first) ||
        !trackPath(secondPath, sizeof(secondPath), kMusicDir, names.playlist, names.second) ||
        !hiddenTrackPath(hiddenPath, sizeof(hiddenPath), kMusicDir, names.playlist, names.newFirst) ||
        !trackPath(newFirstPath, sizeof(newFirstPath), kMusicDir, names.playlist, names.newFirst) ||
        !trackPath(newSecondPath, sizeof(newSecondPath), kMusicDir, names.playlist, names.newSecond))
        return false;

    if (!fs_.rename(firstPath, hiddenPath))
        return false;

    if (!fs_.rename(secondPath, newSecondPath)) {
        fs_.rename(hiddenPath, firstPath);
        return false;
    }

    if (fs_.rename(hiddenPath, newFirstPath))
        return true;

    fs_.rename(newSecondPath, secondPath);
    fs_.rename(hiddenPath, firstPath);
    return false;
}

// Troca de lugar duas faixas trocando os prefixos "NN - " e mantendo os títulos: "01 - x" e "02 - y"
// viram "02 - x" e "01 - y".
void Portal::doSwap() {
    if (!claim())
        return;

    trackswap::SwapNames names;
    if (!readSwapQuery(names))
        return sendError(kBadRequest, kErrName);

    if (!trackExists(names.playlist, names.first) || !trackExists(names.playlist, names.second))
        return sendError(kNotFound, kErrName);

    if (!trackswap::prefixesCanSwap(names.first, names.second))
        return sendError(kBadRequest, kErrNumber);

    if (!trackswap::buildSwappedNames(names))
        return sendError(kBadRequest, kErrName);

    const bool firstTaken = trackExists(names.playlist, names.newFirst) && strcmp(names.newFirst, names.second) != 0;
    const bool secondTaken = trackExists(names.playlist, names.newSecond) && strcmp(names.newSecond, names.first) != 0;
    if (firstTaken || secondTaken)
        return sendError(kConflict, kErrExists);

    changed_ = true;
    if (!swapFiles(names))
        return sendError(kServerError, kErrIo);

    sendOk(false);
}
