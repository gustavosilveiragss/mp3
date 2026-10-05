#include "Media.h"
#include "Portal.h"

#include "PortalPaths.h"
#include "PortalQuery.h"

using namespace portal;
using namespace portalhttp;

namespace {

constexpr char kOctetStream[] = "application/octet-stream";

} // namespace

// Larga o envio em curso e guarda o motivo em last para a página consultar quando a conexão cair.
void Portal::abandonUpload(const char* reason, bool closeSocket) {
    strlcpy(last_, reason, sizeof(last_));
    upload_.open = false;
    clearProgress();
    dropNewDir();

    if (closeSocket)
        closeConnection();
}

// O timeout zerado faz o laço do WebServer desistir do socket na hora.
void Portal::closeConnection() {
    upload_.replied = true;
    WiFiClient& client = mrm::currentClient(*server_);
    client.stop();
    client.setTimeout(0);
}

// Responde e fecha o socket sem ler o resto do corpo.
void Portal::reject(int code, const char* error) {
    PDBG(mrm::log("portal recusa ", code, " ", error));
    mrm::crashlog::note("up recusado %d %s", code, error);
    abandonUpload(error, false);
    sendError(code, error);
    closeConnection();
}

void Portal::rejectSink(mrm::UploadSink::Error error) {
    using Error = mrm::UploadSink::Error;
    switch (error) {
    case Error::Name:
        return reject(kBadRequest, kErrName);
    case Error::Space:
        return reject(kInsufficientStorage, kErrSpace);
    case Error::Exists:
        return reject(kConflict, kErrExists);
    case Error::Rejected:
        return reject(kUnsupportedMedia, kErrNotMp3);
    default:
        return reject(kServerError, kErrIo);
    }
}

// A+B no meio de um arquivo: larga o arquivo e a conexão sem resposta.
void Portal::cancelUpload() {
    PDBG(portaldebug::event("cancelado (A+B ou parado)", sink_.written()));
    mrm::crashlog::note("up cancelado bytes=%lu", static_cast<unsigned long>(sink_.written()));
    sink_.abort();
    abandonUpload(kErrAborted, true);
}

// Playlist criada por um upload que falhou não pode ficar vazia na biblioteca.
void Portal::dropNewDir() {
    if (!upload_.newDir)
        return;

    upload_.newDir = false;

    char dir[kPathMax];
    if (playlistDir(dir, sizeof(dir), kMusicDir, query_.playlist) && fs_.exists(dir))
        fs_.rmdir(dir);
}

// A última leitura (menor que HTTP_RAW_BUFLEN) nunca enche e custaria kRecvTimeoutS por arquivo: o timeout encurta.
void Portal::armTimeout(uint32_t remaining) {
    if (remaining < HTTP_RAW_BUFLEN)
        mrm::currentClient(*server_).Stream::setTimeout(kTailTimeoutMs);
}

void Portal::startUpload() {
    strlcpy(last_, kLastOk, sizeof(last_));

    if (server_->header(kHdrType) != kOctetStream)
        return reject(kUnsupportedMedia, kErrIo);

    if (!queryName(query_.text, "p", query_.playlist, sizeof(query_.playlist), mrm::validPlaylistName) ||
        !queryName(query_.text, "n", query_.track, sizeof(query_.track), mrm::validTrackName))
        return reject(kBadRequest, kErrName);

    if (upload_.length == 0 || upload_.length > kMaxFileBytes)
        return reject(kPayloadTooLarge, kErrLimit);

    char dir[kPathMax];
    if (!playlistDir(dir, sizeof(dir), kMusicDir, query_.playlist))
        return reject(kBadRequest, kErrName);
    const bool isNew = !fs_.exists(dir);
    if (targetFull(dir, isNew))
        return reject(kConflict, kErrLimit);

    sniff_.reset();
    const mrm::UploadSink::Error error = sink_.start(fs_, {dir, query_.track, upload_.length});
    upload_.newDir = isNew; // o start cria a pasta
    if (error != mrm::UploadSink::Error::None)
        return rejectSink(error);

    upload_.open = true;
    PDBG(portaldebug::start());
    readProgress();
}

void Portal::readProgress() {
    uint32_t file = 1, files = 1, totalSize = upload_.length, base = 0;
    queryUint(query_.text, "i", file);
    queryUint(query_.text, "of", files);
    queryUint(query_.text, "tb", totalSize);
    queryUint(query_.text, "ts", base);

    progress_ = Progress{};
    progress_.active = true;
    progress_.at = millis();

    progress_.files = static_cast<uint16_t>(constrain(files, 1u, kMaxQueueFiles));
    progress_.file = static_cast<uint16_t>(constrain(file, 1u, uint32_t(progress_.files)));

    progress_.size = upload_.length;
    progress_.totalSize = max(totalSize, base + upload_.length);
    upload_.base = base;
    strlcpy(progress_.name, query_.track, sizeof(progress_.name));
}

void Portal::writeRaw(const HTTPRaw& raw) {
    if (!upload_.open)
        return; // corpo de um POST que não e upload: descartado

    PDBG(const uint32_t startUs = micros());
    const mrm::UploadSink::Error error = sink_.write(raw.buf, raw.currentSize);
    PDBG(portaldebug::sd(startUs, raw.currentSize));

    if (error != mrm::UploadSink::Error::None)
        return rejectSink(error);

    progress_.sent = sink_.written();
    progress_.totalSent = upload_.base + progress_.sent;
    progress_.at = millis();
    touch();

    if (tick_)
        tick_(tickCtx_);
    PDBG(portaldebug::tick());

    if (stop_)
        return cancelUpload();

    armTimeout(upload_.length > raw.totalSize ? upload_.length - raw.totalSize : 0);
}

void Portal::finishUpload() {
    if (!claim())
        return;

    if (!upload_.open)
        return sendError(kBadRequest, kErrIo);

    const mrm::UploadSink::Error error = sink_.finish();
    PDBG(portaldebug::report());

    if (error == mrm::UploadSink::Error::Rejected && sink_.written() != upload_.length)
        return reject(kBadRequest, kErrIo);

    if (error != mrm::UploadSink::Error::None)
        return rejectSink(error);

    upload_.open = false;
    upload_.newDir = false;
    progress_.active = false;

    progress_.more = progress_.file < progress_.files;
    progress_.at = millis();
    progress_.sent = progress_.size;
    progress_.totalSent = upload_.base + progress_.size;

    changed_ = true;
    strlcpy(last_, kLastOk, sizeof(last_));
    mrm::crashlog::note("up ok bytes=%lu heap=%u", static_cast<unsigned long>(progress_.size),
                        static_cast<unsigned>(ESP.getFreeHeap()));

    char body[kUploadBodyMax];
    snprintf(body, sizeof(body), "{\"ok\":1,\"sr\":%lu,\"free\":%llu}", static_cast<unsigned long>(sniff_.sampleRate()),
             static_cast<unsigned long long>(media::freeBytes()));
    sendJson(kOk, body);
}
