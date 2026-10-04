#include "Library.h"
#include "Media.h"
#include "Portal.h"
#include "PortalQuery.h"

// Gerencia o que ja estava na midia: mover, reordenar e listar .mp3 soltos.

// Move um .mp3 (solto ou de outra playlist) para /music/<to>/. Nunca sobrescreve; so rename no mesmo
// FS, entao nao gasta espaco. Playlist de origem que fica vazia some.
void Portal::doMove() {
    if (!claim())
        return;
    char src[kPathMax], dir[kDirMax], dst[kPathMax];
    bool track = false;
    const int code = resolve(src, sizeof(src), track);
    if (code || !track)
        return sendError(code ? code : 400, "name");
    if (portal::queryText(req_.query, "to", req_.target, sizeof(req_.target)) != portal::Param::Ok || !mrm::validPlaylistName(req_.target))
        return sendError(400, "name");
    snprintf(dir, sizeof(dir), "%s/%.*s", kMusicDir, int(mrm::kMaxPlaylistLen), req_.target);
    snprintf(dst, sizeof(dst), "%s/%s", dir, req_.track);
    const bool isNew = !fs_.exists(dir);
    if (!isNew && fs_.exists(dst))
        return sendError(409, "exists");
    if (isNew ? dirlist::count(kMusicDir, true, nullptr) >= Library::kMaxPlaylists : dirlist::count(dir, false, nullptr) >= Library::kMaxTracks)
        return sendError(409, "limit");
    if (isNew && !fs_.mkdir(dir))
        return sendError(500, "io");
    changed_ = true;
    if (!fs_.rename(src, dst)) {
        if (isNew)
            fs_.rmdir(dir);
        return sendError(500, "io");
    }
    if (req_.playlist[0]) {
        snprintf(dir, sizeof(dir), "%s/%s", kMusicDir, req_.playlist);
        if (dirlist::count(dir, false, nullptr) == 0)
            fs_.rmdir(dir); // falha sem barulho se sobrou algo que nao e faixa
    }
    sendOk(false);
}

// Tamanho de "NN - " no comeco do nome (um ou mais digitos, " - "), ou 0.
static size_t numPrefix(const char* name) {
    size_t i = 0;
    while (name[i] >= '0' && name[i] <= '9')
        ++i;
    return i > 0 && strncmp(name + i, " - ", 3) == 0 ? i + 3 : 0;
}

// Troca de lugar duas faixas trocando os prefixos "NN - " e mantendo os titulos: "01 - x" e "02 - y"
// viram "02 - x" e "01 - y". Sem prefixo numerico nos dois (ou prefixos iguais) nao ha o que trocar.
// A: a vira ".na" (o nome final, escondido da biblioteca), b vira nb, ".na" vira na. Queda no meio
// deixa so o oculto, que recoverSwap devolve no boot.
bool Portal::swapFiles(const char* dir, const char* a, const char* b, const char* na, const char* nb) {
    char pa[kPathMax], pb[kPathMax], tmp[kPathMax], pna[kPathMax], pnb[kPathMax];
    snprintf(pa, sizeof(pa), "%s/%s", dir, a);
    snprintf(pb, sizeof(pb), "%s/%s", dir, b);
    snprintf(tmp, sizeof(tmp), "%s/.%s", dir, na);
    snprintf(pna, sizeof(pna), "%s/%s", dir, na);
    snprintf(pnb, sizeof(pnb), "%s/%s", dir, nb);
    if (!fs_.rename(pa, tmp))
        return false;
    if (!fs_.rename(pb, pnb)) {
        fs_.rename(tmp, pa);
        return false;
    }
    if (fs_.rename(tmp, pna))
        return true;
    fs_.rename(pnb, pb);
    fs_.rename(tmp, pa);
    return false;
}

void Portal::doSwap() {
    if (!claim())
        return;
    using portal::Param;
    char dir[kDirMax], pa[kPathMax], na[kTrackBuf], nb[kTrackBuf];
    if (portal::queryText(req_.query, "p", req_.playlist, sizeof(req_.playlist)) != Param::Ok || !mrm::validPlaylistName(req_.playlist) ||
        portal::queryText(req_.query, "a", req_.track, sizeof(req_.track)) != Param::Ok || !mrm::validTrackName(req_.track) ||
        portal::queryText(req_.query, "b", req_.target, sizeof(req_.target)) != Param::Ok || !mrm::validTrackName(req_.target) ||
        strcmp(req_.track, req_.target) == 0)
        return sendError(400, "name");
    const char *a = req_.track, *b = req_.target;
    snprintf(dir, sizeof(dir), "%s/%s", kMusicDir, req_.playlist);
    snprintf(pa, sizeof(pa), "%s/%s", dir, a);
    if (!fs_.exists(pa))
        return sendError(404, "name");
    snprintf(pa, sizeof(pa), "%s/%s", dir, b);
    if (!fs_.exists(pa))
        return sendError(404, "name");
    const size_t la = numPrefix(a), lb = numPrefix(b);
    if (!la || !lb || (la == lb && strncmp(a, b, la) == 0))
        return sendError(400, "num");
    const int x = snprintf(na, sizeof(na), "%.*s%s", int(lb), b, a + la);
    const int y = snprintf(nb, sizeof(nb), "%.*s%s", int(la), a, b + lb);
    if (x >= int(sizeof(na)) || y >= int(sizeof(nb)) || !mrm::validTrackName(na) || !mrm::validTrackName(nb))
        return sendError(400, "name");
    snprintf(pa, sizeof(pa), "%s/%s", dir, na);
    if (fs_.exists(pa) && strcmp(na, b) != 0)
        return sendError(409, "exists");
    snprintf(pa, sizeof(pa), "%s/%s", dir, nb);
    if (fs_.exists(pa) && strcmp(nb, a) != 0)
        return sendError(409, "exists");
    changed_ = true;
    if (!swapFiles(dir, a, b, na, nb))
        return sendError(500, "io");
    sendOk(false);
}

// .mp3 soltos: os de /music e os da raiz da midia, ate kMaxLoose, cada origem em ordem de nome.
void Portal::sendLoose() {
    touch();
    server_->sendHeader("Cache-Control", "no-store");
    server_->setContentLength(CONTENT_LENGTH_UNKNOWN);
    server_->send(200, "application/json", "");
    mrm::JsonOut json(*server_);
    char item[24];
    uint8_t total = 0;
    bool more = false;
    json.raw("{\"t\":[");
    for (const bool root : {false, true}) {
        char after[kTrackBuf] = "";
        uint8_t count = 0;
        while (!more && nextChunk(root ? "/" : kMusicDir, false, after, count)) {
            for (uint8_t i = 0; i < count && !more; ++i) {
                more = total >= kMaxLoose;
                if (more)
                    break;
                json.raw(total ? ",{\"n\":" : "{\"n\":");
                json.str(chunk_[i]);
                snprintf(item, sizeof(item), ",\"b\":%lu,\"s\":", static_cast<unsigned long>(chunkSize_[i]));
                json.raw(item);
                json.str(root ? "root" : "music");
                json.raw("}");
                ++total;
            }
        }
    }
    json.raw(more ? "],\"more\":1}" : "],\"more\":0}");
    json.end();
}
