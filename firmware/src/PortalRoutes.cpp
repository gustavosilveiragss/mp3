#include <uri/UriGlob.h>

#include "Media.h"
#include "Portal.h"

#include "PortalQuery.h"

using namespace portal;
using namespace portalhttp;

namespace {

constexpr char kHost[] = "192.168.4.1"; // contra DNS rebinding: o portal só atende por este nome
constexpr char kUploadUri[] = "/api/upload";
constexpr char kApiPrefix[] = "/api/";
constexpr char kMultipart[] = "multipart/";
constexpr char kHdrMrm[] = "X-Mrm";
constexpr char kHdrQuery[] = "X-Q"; // query do POST: o caminho raw do WebServer não a entrega
constexpr char kHdrLength[] = "Content-Length";

uint32_t parseUint(const char* text) {
    char* end = nullptr;
    const unsigned long v = strtoul(text, &end, 10);
    return end == text ? 0 : static_cast<uint32_t>(min<unsigned long>(v, UINT32_MAX));
}

} // namespace

void Portal::registerRoutes(WebServer& server) {
    server_ = &server;

    static const char* headers[] = {kHdrMrm, kHdrQuery, kHdrType, kHdrLength};
    server.collectHeaders(headers, sizeof(headers) / sizeof(headers[0]));

    server.on("/api/state", HTTP_GET, [this] { sendState(); });
    server.on("/api/tracks", HTTP_GET, [this] { sendTracks(); });
    server.on("/api/loose", HTTP_GET, [this] { sendLoose(); });

    server.on(kUploadUri, HTTP_POST, [this] { finishUpload(); }, [this] { onRaw(); });
    server.on("/api/delete", HTTP_POST, [this] { doDelete(); }, [this] { onRaw(); });
    server.on("/api/rename", HTTP_POST, [this] { doRename(); }, [this] { onRaw(); });
    server.on("/api/move", HTTP_POST, [this] { doMove(); }, [this] { onRaw(); });
    server.on("/api/swap", HTTP_POST, [this] { doSwap(); }, [this] { onRaw(); });
    server.on("/api/done", HTTP_POST, [this] { doDone(); }, [this] { onRaw(); });

    // Todo método com corpo precisa de handler raw: sem ele o WebServer aloca o corpo inteiro na RAM.
    // "*" cobre alvo sem "/" inicial (PUT http://x/a, POST *), que "/*" não casa.
    for (const HTTPMethod method : {HTTP_POST, HTTP_PUT, HTTP_PATCH, HTTP_DELETE}) {
        server.on(UriGlob("/*"), method, [this] { notFound(); }, [this] { onRaw(); });
        server.on(UriGlob("*"), method, [this] { notFound(); }, [this] { onRaw(); });
    }
}

void Portal::sendJson(int code, const char* body) {
    server_->sendHeader("Cache-Control", "no-store");
    server_->setContentLength(CONTENT_LENGTH_NOT_SET); // dentro do raw o valor vem do pedido anterior
    server_->send(code, kJsonType, body);
}

void Portal::sendError(int code, const char* error) {
    char body[kErrorBodyMax];
    snprintf(body, sizeof(body), "{\"e\":\"%s\"}", error);
    sendJson(code, body);
}

void Portal::sendOk(bool withFree) {
    char body[kOkBodyMax];

    if (withFree)
        snprintf(body, sizeof(body), "{\"ok\":1,\"free\":%llu}", static_cast<unsigned long long>(media::freeBytes()));
    else
        strlcpy(body, "{\"ok\":1}", sizeof(body));

    sendJson(kOk, body);
}

// Os handlers só agem sobre um pedido cujo corpo passou por startRaw: um POST multipart sem arquivo
// nunca chega lá, e agiria com o upload_ do pedido anterior. Devolve false (e consome a marca) senão.
bool Portal::claim() {
    const bool fresh = upload_.started && !server_->header(kHdrType).startsWith(kMultipart);
    upload_.started = false;

    if (!fresh)
        sendError(kForbidden, kErrIo);

    return fresh && !upload_.replied;
}

void Portal::notFound() {
    if (claim())
        sendError(kNotFound, kErrIo);
}

void Portal::doDone() {
    if (!claim())
        return;

    sendOk(false);
    done_ = true;
    doneAt_ = millis();
}

bool Portal::readQuery() {
    auto query = server_->header(kHdrQuery);
    if (query.length() >= sizeof(query_.text))
        return false;

    strlcpy(query_.text, query.c_str(), sizeof(query_.text));

    return true;
}

void Portal::onRaw() {
    if (server_->header(kHdrType).startsWith(kMultipart)) {
        upload_.started = false;             // o handler final não pode agir com o upload_ de outro pedido
        mrm::currentClient(*server_).stop(); // só aceitamos corpo cru
        return;
    }

    const HTTPRaw& raw = server_->raw();
    switch (raw.status) {
    case RAW_START:
        return startRaw();

    case RAW_WRITE:
        if (!upload_.replied)
            writeRaw(raw);
        return;

    case RAW_ABORTED:
        PDBG(portaldebug::event("raw abortado", raw.totalSize));
        upload_.started = false;
        if (upload_.open) {
            sink_.abort();
            abandonUpload(kErrAborted, false);
        }

        clearProgress();
        return;
    default:
        return;
    }
}

// A página cruza last com este token: precisa ser lido antes de qualquer reject.
void Portal::readUploadToken() {
    lastU_ = 0;
    if (readQuery())
        queryUint(query_.text, "u", lastU_);
}

void Portal::startRaw() {
    touch();

    memset(&query_, 0, sizeof(query_));
    memset(&upload_, 0, sizeof(upload_));
    upload_.started = true;

    const bool upload = server_->uri() == kUploadUri;
    if (upload)
        readUploadToken();

    if (stop_)
        return reject(kUnavailable, kErrAborted);

    mrm::setRecvTimeout(*server_, kRecvTimeoutS);

    if (server_->header(kHdrMrm) != "1" || server_->hostHeader() != kHost)
        return reject(kForbidden, kErrIo);

    if (server_->method() != HTTP_POST)
        return reject(kMethodNotAllowed, kErrIo);

    upload_.length = parseUint(server_->header(kHdrLength).c_str());
    armTimeout(upload_.length);

    if (!upload && !server_->uri().startsWith(kApiPrefix))
        return reject(kNotFound, kErrIo);

    if (!readQuery())
        return reject(kBadRequest, kErrName);

    if (upload)
        startUpload();
    else if (upload_.length > kMaxActionBody)
        reject(kPayloadTooLarge, kErrLimit);
}
