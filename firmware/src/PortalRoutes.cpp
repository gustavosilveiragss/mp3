#include <uri/UriGlob.h>

#include "Library.h"
#include "Media.h"
#include "Portal.h"
#include "PortalQuery.h"

namespace {

constexpr char kHost[] = "192.168.4.1"; // contra DNS rebinding: o portal so atende por este nome
constexpr char kHdrMrm[] = "X-Mrm";
constexpr char kHdrQuery[] = "X-Q"; // query do POST: o caminho raw do WebServer nao a entrega
constexpr char kHdrType[] = "Content-Type";
constexpr char kHdrLength[] = "Content-Length";

// O WebServer le o corpo do raw com o timeout do seu _currentClient (1 s) e client() devolve uma
// copia, entao setTimeout/stop por ali nao chegam ao socket. O membro e protected: o truque do
// ponteiro de membro em instanciacao explicita acessa sem alterar a lib.
struct ClientTag {
    using Type = WiFiClient WebServer::*;
    friend Type clientMember(ClientTag);
};
template <typename Tag, typename Tag::Type Member>
struct Reach {
    friend typename Tag::Type clientMember(Tag) { return Member; }
};
template struct Reach<ClientTag, &WebServer::_currentClient>;

WiFiClient& clientOf(WebServer& server) {
    return server.*clientMember(ClientTag{});
}

uint32_t parseUint(const char* text) {
    char* end = nullptr;
    const unsigned long v = strtoul(text, &end, 10);
    return end == text ? 0 : static_cast<uint32_t>(min<unsigned long>(v, UINT32_MAX));
}

} // namespace

void Portal::registerRoutes(WebServer& server) {
    server_ = &server;
    static const char* headers[] = {kHdrMrm, kHdrQuery, kHdrType, kHdrLength};
    server.collectHeaders(headers, 4);
    server.on("/api/state", HTTP_GET, [this] { sendState(); });
    server.on("/api/tracks", HTTP_GET, [this] { sendTracks(); });
    server.on("/api/loose", HTTP_GET, [this] { sendLoose(); });
    server.on("/api/upload", HTTP_POST, [this] { finishUpload(); }, [this] { onRaw(); });
    server.on("/api/delete", HTTP_POST, [this] { doDelete(); }, [this] { onRaw(); });
    server.on("/api/rename", HTTP_POST, [this] { doRename(); }, [this] { onRaw(); });
    server.on("/api/move", HTTP_POST, [this] { doMove(); }, [this] { onRaw(); });
    server.on("/api/swap", HTTP_POST, [this] { doSwap(); }, [this] { onRaw(); });
    server.on("/api/done", HTTP_POST, [this] { doDone(); }, [this] { onRaw(); });
    // Todo metodo com corpo precisa de handler raw: sem ele o WebServer aloca o corpo inteiro na RAM.
    // "*" cobre alvo sem "/" inicial (PUT http://x/a, POST *), que "/*" nao casa.
    for (const HTTPMethod method : {HTTP_POST, HTTP_PUT, HTTP_PATCH, HTTP_DELETE}) {
        server.on(UriGlob("/*"), method, [this] { notFound(); }, [this] { onRaw(); });
        server.on(UriGlob("*"), method, [this] { notFound(); }, [this] { onRaw(); });
    }
}

void Portal::sendJson(int code, const char* body) {
    server_->sendHeader("Cache-Control", "no-store");
    server_->setContentLength(CONTENT_LENGTH_NOT_SET); // dentro do raw o valor vem do pedido anterior
    server_->send(code, "application/json", body);
}

void Portal::sendError(int code, const char* error) {
    char body[32];
    snprintf(body, sizeof(body), "{\"e\":\"%s\"}", error);
    sendJson(code, body);
}

void Portal::sendOk(bool withFree) {
    char body[48];
    if (withFree)
        snprintf(body, sizeof(body), "{\"ok\":1,\"free\":%llu}", static_cast<unsigned long long>(media::freeBytes()));
    else
        strlcpy(body, "{\"ok\":1}", sizeof(body));
    sendJson(200, body);
}

// Os handlers so agem sobre um pedido cujo corpo passou por startRaw: um POST multipart sem arquivo
// nunca chega la, e agiria com o req_ do pedido anterior. Devolve false (e consome a marca) senao.
bool Portal::claim() {
    const bool fresh = req_.started && !server_->header(kHdrType).startsWith("multipart/");
    req_.started = false;
    if (!fresh)
        sendError(403, "io");
    return fresh && !req_.replied;
}

void Portal::notFound() {
    if (claim())
        sendError(404, "io");
}

// Responde e fecha o socket sem ler o resto do corpo; o motivo fica em last para a pagina
// consultar quando a conexao cair. O timeout zerado faz o laco do WebServer desistir na hora.
void Portal::reject(int code, const char* error) {
    strlcpy(last_, error, sizeof(last_));
    req_.upload = false;
    progress_.active = false;
    dropNewDir();
    sendError(code, error);
    req_.replied = true;
    WiFiClient& client = clientOf(*server_);
    client.stop();
    client.setTimeout(0);
}

void Portal::rejectSink(mrm::UploadSink::Error error) {
    using Error = mrm::UploadSink::Error;
    switch (error) {
    case Error::Name:
        return reject(400, "name");
    case Error::Space:
        return reject(507, "space");
    case Error::Exists:
        return reject(409, "exists");
    case Error::Rejected:
        return reject(415, "notmp3");
    default:
        return reject(500, "io");
    }
}

// A+B no meio de um arquivo: larga o arquivo e a conexao sem resposta.
void Portal::cancelUpload() {
    sink_.abort();
    strlcpy(last_, "aborted", sizeof(last_));
    req_.upload = false;
    req_.replied = true;
    progress_.active = false;
    dropNewDir();
    WiFiClient& client = clientOf(*server_);
    client.stop();
    client.setTimeout(0);
}

// Playlist criada por um upload que falhou nao pode ficar vazia na biblioteca.
void Portal::dropNewDir() {
    if (!req_.newDir)
        return;
    req_.newDir = false;
    char dir[kPathMax];
    snprintf(dir, sizeof(dir), "%s/%s", kMusicDir, req_.playlist);
    if (fs_.exists(dir))
        fs_.rmdir(dir);
}

bool Portal::readQuery() {
    auto query = server_->header(kHdrQuery);
    if (query.length() >= sizeof(req_.query))
        return false;
    strlcpy(req_.query, query.c_str(), sizeof(req_.query));
    return true;
}

void Portal::onRaw() {
    if (server_->header(kHdrType).startsWith("multipart/")) {
        req_.started = false;      // o handler final nao pode agir com o req_ de outro pedido
        clientOf(*server_).stop(); // so aceitamos corpo cru
        return;
    }
    const HTTPRaw& raw = server_->raw();
    switch (raw.status) {
    case RAW_START:
        return startRaw();
    case RAW_WRITE:
        if (!req_.replied)
            writeRaw(raw);
        return;
    case RAW_ABORTED:
        req_.started = false;
        if (req_.upload) {
            sink_.abort();
            strlcpy(last_, "aborted", sizeof(last_));
            req_.upload = false;
            dropNewDir();
        }
        progress_.active = false;
        return;
    default:
        return;
    }
}

void Portal::startRaw() {
    touch();
    memset(&req_, 0, sizeof(req_));
    req_.started = true;
    if (stop_)
        return reject(503, "aborted");
    clientOf(*server_).setTimeout(kRecvTimeoutS);
    if (server_->header(kHdrMrm) != "1" || server_->hostHeader() != kHost)
        return reject(403, "io");
    if (server_->method() != HTTP_POST)
        return reject(405, "io");
    req_.length = parseUint(server_->header(kHdrLength).c_str());
    armTimeout(req_.length);
    const bool upload = server_->uri() == "/api/upload";
    if (!upload && !server_->uri().startsWith("/api/"))
        return reject(404, "io");
    if (!readQuery())
        return reject(400, "name");
    if (upload)
        startUpload();
    else if (req_.length > kMaxActionBody)
        reject(413, "limit");
}

// O core pede 1436 B por leitura e espera encher ou estourar o timeout: na ultima leitura, que nunca
// enche, isso custaria kRecvTimeoutS por arquivo. Faltando menos que um bloco, o timeout encurta.
void Portal::armTimeout(uint32_t remaining) {
    if (remaining < HTTP_RAW_BUFLEN)
        clientOf(*server_).Stream::setTimeout(kTailTimeoutMs);
}

void Portal::startUpload() {
    using portal::Param;
    strlcpy(last_, "ok", sizeof(last_));
    if (server_->header(kHdrType) != "application/octet-stream")
        return reject(415, "io");
    if (portal::queryText(req_.query, "p", req_.playlist, sizeof(req_.playlist)) != Param::Ok || !mrm::validPlaylistName(req_.playlist) ||
        portal::queryText(req_.query, "n", req_.track, sizeof(req_.track)) != Param::Ok || !mrm::validTrackName(req_.track))
        return reject(400, "name");
    if (req_.length == 0 || req_.length > kMaxFileBytes)
        return reject(413, "limit");

    char dir[kPathMax];
    snprintf(dir, sizeof(dir), "%s/%s", kMusicDir, req_.playlist);
    const bool isNew = !fs_.exists(dir);
    const bool full = isNew ? scanDir(kMusicDir, true, nullptr) >= Library::kMaxPlaylists : scanDir(dir, false, nullptr) >= Library::kMaxTracks;
    if (full)
        return reject(409, "limit");
    sniff_.reset();
    const mrm::UploadSink::Error error = sink_.start(fs_, {dir, req_.track, req_.length});
    req_.newDir = isNew; // o start cria a pasta
    if (error != mrm::UploadSink::Error::None)
        return rejectSink(error);
    req_.upload = true;
    readProgress();
}

void Portal::readProgress() {
    using portal::queryUint;
    uint32_t file = 1, files = 1, totalSize = req_.length, base = 0;
    queryUint(req_.query, "i", file);
    queryUint(req_.query, "of", files);
    queryUint(req_.query, "tb", totalSize);
    queryUint(req_.query, "ts", base);
    progress_ = Progress{};
    progress_.active = true;
    progress_.files = static_cast<uint16_t>(constrain(files, 1u, 65535u));
    progress_.file = static_cast<uint16_t>(constrain(file, 1u, uint32_t(progress_.files)));
    progress_.size = req_.length;
    progress_.totalSize = max(totalSize, base + req_.length);
    req_.base = base;
    strlcpy(progress_.name, req_.track, sizeof(progress_.name));
}

void Portal::writeRaw(const HTTPRaw& raw) {
    if (!req_.upload)
        return; // corpo de um POST que nao e upload: descartado
    const mrm::UploadSink::Error error = sink_.write(raw.buf, raw.currentSize);
    if (error != mrm::UploadSink::Error::None)
        return rejectSink(error);
    progress_.sent = sink_.written();
    progress_.totalSent = req_.base + progress_.sent;
    touch();
    if (tick_)
        tick_(tickCtx_);
    if (stop_)
        return cancelUpload();
    armTimeout(req_.length > raw.totalSize ? req_.length - raw.totalSize : 0);
}

void Portal::finishUpload() {
    if (!claim())
        return;
    if (!req_.upload)
        return sendError(400, "io");
    const mrm::UploadSink::Error error = sink_.finish();
    if (error == mrm::UploadSink::Error::Rejected && sink_.written() != req_.length)
        return reject(400, "io");
    if (error != mrm::UploadSink::Error::None)
        return rejectSink(error);
    req_.upload = false;
    req_.newDir = false;
    progress_.active = false;
    progress_.sent = progress_.size;
    changed_ = true;
    strlcpy(last_, "ok", sizeof(last_));
    char body[64];
    snprintf(body, sizeof(body), "{\"ok\":1,\"sr\":%lu,\"free\":%llu}", static_cast<unsigned long>(sniff_.sampleRate()), static_cast<unsigned long long>(media::freeBytes()));
    sendJson(200, body);
}

// "music" (padrao) ou "root": pasta dos .mp3 soltos, sem a barra final.
bool Portal::looseBase(char* base, size_t cap) {
    char src[8];
    switch (portal::queryText(req_.query, "src", src, sizeof(src))) {
    case portal::Param::Missing:
        strlcpy(base, kMusicDir, cap);
        return true;
    case portal::Param::Ok:
        if (strcmp(src, "music") == 0)
            strlcpy(base, kMusicDir, cap);
        else if (strcmp(src, "root") == 0)
            base[0] = '\0';
        else
            return false;
        return true;
    default:
        return false;
    }
}

// p (e n opcional) do X-Q como caminho dentro de /music; p vazio com n e um .mp3 solto (src).
// Devolve 0, ou o codigo HTTP do erro.
int Portal::resolve(char* path, size_t cap, bool& track) {
    using portal::Param;
    if (portal::queryText(req_.query, "p", req_.playlist, sizeof(req_.playlist)) != Param::Ok)
        return 400;
    const Param n = portal::queryText(req_.query, "n", req_.track, sizeof(req_.track));
    track = n == Param::Ok;
    if (n == Param::Bad || (track && !mrm::validTrackName(req_.track)))
        return 400;
    if (!req_.playlist[0]) {
        char base[sizeof(kMusicDir)];
        if (!track || !looseBase(base, sizeof(base)))
            return 400;
        snprintf(path, cap, "%s/%s", base, req_.track);
        return fs_.exists(path) ? 0 : 404;
    }
    if (!mrm::validPlaylistName(req_.playlist))
        return 400;
    if (track)
        snprintf(path, cap, "%s/%s/%s", kMusicDir, req_.playlist, req_.track);
    else
        snprintf(path, cap, "%s/%s", kMusicDir, req_.playlist);
    return fs_.exists(path) ? 0 : 404;
}

void Portal::doDelete() {
    if (!claim())
        return;
    char path[kPathMax];
    bool track = false;
    const int code = resolve(path, sizeof(path), track);
    if (code)
        return sendError(code, "name");
    changed_ = true; // mesmo que o apagar falhe no meio, o Player precisa reler
    if (!(track ? fs_.remove(path) : purge(path)))
        return sendError(500, "io");
    sendOk(true);
}

void Portal::doRename() {
    if (!claim())
        return;
    char src[kPathMax], dst[kPathMax];
    bool track = false;
    const int code = resolve(src, sizeof(src), track);
    if (code)
        return sendError(code, "name");
    if (portal::queryText(req_.query, "to", req_.target, sizeof(req_.target)) != portal::Param::Ok || !(track ? mrm::validTrackName(req_.target) : mrm::validPlaylistName(req_.target)))
        return sendError(400, "name");
    if (track)
        snprintf(dst, sizeof(dst), "%s/%s/%s", kMusicDir, req_.playlist, req_.target);
    else
        snprintf(dst, sizeof(dst), "%s/%s", kMusicDir, req_.target);
    if (fs_.exists(dst))
        return sendError(409, "exists");
    if (!fs_.rename(src, dst))
        return sendError(500, "io");
    changed_ = true;
    sendOk(false);
}

void Portal::doDone() {
    if (!claim())
        return;
    sendOk(false);
    done_ = true;
    doneAt_ = millis();
}

void Portal::sendState() {
    touch();
    server_->sendHeader("Cache-Control", "no-store");
    server_->setContentLength(CONTENT_LENGTH_UNKNOWN);
    server_->send(200, "application/json", "");
    mrm::JsonOut json(*server_);
    char buf[128];
    json.raw("{\"lang\":");
    json.str(mrm::Locale::code(locale_.lang()));
    json.raw(",\"name\":");
    json.str(ssid_);
    snprintf(buf, sizeof(buf), ",\"free\":%llu,\"total\":%llu,\"reserve\":%lu,", static_cast<unsigned long long>(media::freeBytes()), static_cast<unsigned long long>(media::totalBytes()), static_cast<unsigned long>(kReserveBytes));
    json.raw(buf);
    snprintf(buf, sizeof(buf), "\"lim\":{\"pl\":%u,\"tr\":%u,\"plLen\":%u,\"trLen\":%u,\"file\":%lu},\"pl\":[", Library::kMaxPlaylists, Library::kMaxTracks, unsigned(mrm::kMaxPlaylistLen), unsigned(mrm::kMaxTrackLen), static_cast<unsigned long>(kMaxFileBytes));
    json.raw(buf);
    writePlaylists(json);
    json.raw("],\"last\":");
    json.str(last_);
    json.raw("}");
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
            snprintf(path, sizeof(path), "%s/%s", kMusicDir, chunk_[i]);
            const uint16_t tracks = scanDir(path, false, &bytes);
            json.raw(first ? "{\"n\":" : ",{\"n\":");
            json.str(chunk_[i]);
            snprintf(path, sizeof(path), ",\"c\":%u,\"b\":%lu}", tracks, static_cast<unsigned long>(bytes));
            json.raw(path);
            first = false;
        }
    }
}

void Portal::sendTracks() {
    touch();
    char playlist[kPlaylistBuf], dir[kPathMax];
    if (server_->arg("p").length() >= sizeof(playlist))
        return sendError(400, "name");
    strlcpy(playlist, server_->arg("p").c_str(), sizeof(playlist));
    snprintf(dir, sizeof(dir), "%s/%s", kMusicDir, playlist);
    File d = mrm::validPlaylistName(playlist) ? fs_.open(dir) : File();
    if (!d || !d.isDirectory())
        return sendError(404, "name");
    d.close();

    server_->sendHeader("Cache-Control", "no-store");
    server_->setContentLength(CONTENT_LENGTH_UNKNOWN);
    server_->send(200, "application/json", "");
    mrm::JsonOut json(*server_);
    char after[kTrackBuf] = "";
    char item[24];
    uint8_t count = 0;
    bool first = true;
    json.raw("{\"p\":");
    json.str(playlist);
    json.raw(",\"t\":[");
    while (nextChunk(dir, false, after, count)) {
        for (uint8_t i = 0; i < count; ++i) {
            json.raw(first ? "{\"n\":" : ",{\"n\":");
            json.str(chunk_[i]);
            snprintf(item, sizeof(item), ",\"b\":%lu}", static_cast<unsigned long>(chunkSize_[i]));
            json.raw(item);
            first = false;
        }
    }
    json.raw("]}");
    json.end();
}
