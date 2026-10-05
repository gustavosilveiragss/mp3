#include "Portal.h"

#include <WiFi.h>

#include "DirWindow.h"
#include "Library.h"
#include "Media.h"

namespace {

uint64_t freeBytes(void*) {
    return media::freeBytes();
}

// Mp3Sniff e UploadSink falam o mesmo, em enums diferentes.
mrm::UploadSink::Verdict sniffVerdict(void* sniff, const uint8_t* data, size_t size) {
    using Verdict = mrm::UploadSink::Verdict;
    switch (static_cast<mrm::Mp3Sniff*>(sniff)->feed(data, size)) {
    case mrm::Mp3Sniff::Result::Ok:
        return Verdict::Ok;
    case mrm::Mp3Sniff::Result::Bad:
        return Verdict::Bad;
    default:
        return Verdict::Need;
    }
}

// Sem /upload embutido (destPath nulo): os envios entram pelas rotas do Portal. Um cliente so.
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
    config.freeBytes = freeBytes;
    config.reserve = Portal::kReserveBytes;
    config.check = sniffVerdict;
    config.checkCtx = sniff;
    return config;
}

} // namespace

Portal::Portal(fs::FS& fs, const mrm::Locale& locale)
    : fs_(fs)
    , locale_(locale)
    , wifi_(wifiConfig(ssid_))
    , pipe_(sinkConfig(&sniff_)) {
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
    strlcpy(last_, "ok", sizeof(last_));
    changed_ = stop_ = done_ = false;
    clients_ = 0;
    idleAt_ = clientsAt_ = millis();
    wifi_.begin();
    pipe_.begin();
    active_ = true;
}

void Portal::handle() {
    if (!active_)
        return;
    const uint32_t now = millis();
    if (now - clientsAt_ >= 500) {
        clientsAt_ = now;
        clients_ = static_cast<uint8_t>(WiFi.softAPgetStationNum());
    }
    if (now - idleAt_ >= kIdleStopMs)
        stop_ = true;
    if (progress_.active && now - progress_.at >= kStallMs)
        cancelUpload(); // celular sumiu sem fechar a conexao: a tela nao fica "recebendo" para sempre
    wifi_.handle();
}

void Portal::end() {
    if (!active_)
        return;
    if (req_.upload)
        pipe_.abort();
    req_.upload = false;
    pipe_.end();
    wifi_.end();
    progress_.active = progress_.more = false;
    active_ = false;
}

bool Portal::doneRequested() const {
    return stop_ || (done_ && millis() - doneAt_ >= kDoneGraceMs);
}

// Ate kChunk nomes depois de `after`, em ordem. Como o diretorio nao vem ordenado e a RAM e curta,
// a lista sai em blocos: cada passada guarda os menores que ainda nao foram entregues.
uint8_t Portal::listChunk(const char* dir, bool dirs, const char* after) {
    return dirlist::chunk({dir, dirs, after, true}, {chunk_, chunkSize_, kChunk});
}

// `after` guarda o ultimo nome entregue; comece com "".
bool Portal::nextChunk(const char* dir, bool dirs, char* after, uint8_t& count) {
    count = listChunk(dir, dirs, after);
    if (count)
        strlcpy(after, chunk_[count - 1], kTrackBuf);
    return count > 0;
}
