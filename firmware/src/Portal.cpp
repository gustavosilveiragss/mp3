#include "Portal.h"

#include <WiFi.h>

#include "DirWindow.h"
#include "Library.h"
#include "Media.h"

namespace {

mrm::UploadVerdict sniffCheck(void* sniff, const uint8_t* data, size_t size) {
    return static_cast<mrm::Mp3Sniff*>(sniff)->feed(data, size);
}

// Sem /upload embutido (destPath nulo): os envios entram pelas rotas do Portal. Um cliente só.
mrm::WifiPortal::Config wifiConfig(const char* ssid) {
    mrm::WifiPortal::Config config;
    config.ssid = ssid;
    config.destPath = nullptr;
    config.pageGz = kPortalPageGz;
    config.pageGzLen = kPortalPageGzLen;
    config.maxClients = 1;
    return config;
}

mrm::UploadSink::Config sinkConfig(mrm::Mp3Sniff* sniff) {
    mrm::UploadSink::Config config;
    config.tmpPath = kTmpPath;
    config.freeBytes = media::freeBytes;
    config.reserve = Portal::kReserveBytes;
    config.check = sniffCheck;
    config.checkCtx = sniff;
    return config;
}

} // namespace

Portal::Portal(fs::FS& fs, const mrm::Locale& locale)
    : fs_(fs)
    , locale_(locale)
    , wifi_(wifiConfig(ssid_))
    , sink_(sinkConfig(&sniff_)) {
    wifi_.onRoutes([this](WebServer& server) { registerRoutes(server); });
}

void Portal::begin(const char* ssid) {
    strlcpy(ssid_, ssid, sizeof(ssid_));

    if (!fs_.exists(kMusicDir))
        fs_.mkdir(kMusicDir);

    mrm::UploadSink::sweep(fs_, kTmpPath);
    recoverSwap();
    sweepEmpty();

    progress_ = Progress{};
    strlcpy(last_, portalhttp::kLastOk, sizeof(last_));
    changed_ = stop_ = done_ = false;
    clients_ = 0;
    idleAt_ = clientsAt_ = millis();

    wifi_.begin();
    active_ = true;
    mrm::crashlog::note("portal on heap=%u", static_cast<unsigned>(ESP.getFreeHeap()));
}

void Portal::handle() {
    if (!active_)
        return;

    const uint32_t now = millis();
    if (now - clientsAt_ >= kClientsPollMs) {
        clientsAt_ = now;
        clients_ = static_cast<uint8_t>(WiFi.softAPgetStationNum());
    }

    if (now - idleAt_ >= kIdleStopMs)
        stop_ = true;

    if (progress_.active && now - progress_.at >= kStallMs)
        cancelUpload(); // celular sumiu sem fechar a conexão: a tela não fica "recebendo" para sempre

    wifi_.handle();
}

void Portal::end() {
    if (!active_)
        return;

    if (upload_.open)
        sink_.abort();

    upload_.open = false;
    wifi_.end();
    clearProgress();
    active_ = false;
    mrm::crashlog::note("portal off minheap=%u", static_cast<unsigned>(ESP.getMinFreeHeap()));
}

bool Portal::doneRequested() const {
    return stop_ || (done_ && millis() - doneAt_ >= kDoneGraceMs);
}

// Até kChunk nomes depois de `after`, em ordem. Como o diretório não vem ordenado e a RAM é curta,
// a lista sai em blocos: cada passada guarda os menores que ainda não foram entregues.
uint8_t Portal::listChunk(const char* dir, bool dirs, const char* after) {
    return dirlist::chunk({dir, dirs, after, true}, {chunk_, chunkSize_, kChunk});
}

// `after` guarda o último nome entregue. Comece com "".
bool Portal::nextChunk(const char* dir, bool dirs, char* after, uint8_t& count) {
    count = listChunk(dir, dirs, after);
    if (count)
        strlcpy(after, chunk_[count - 1], kTrackBuf);

    return count > 0;
}
