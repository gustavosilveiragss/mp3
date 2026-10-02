#include "Portal.h"

#include <WiFi.h>

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

// Mesma ordem do Library: sem diferenciar caixa, com desempate pelos bytes.
int order(const char* a, const char* b) {
    const int c = strcasecmp(a, b);
    return c ? c : strcmp(a, b);
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
    strlcpy(last_, "ok", sizeof(last_));
    changed_ = stop_ = done_ = false;
    clients_ = 0;
    idleAt_ = clientsAt_ = millis();
    wifi_.begin();
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
    wifi_.handle();
}

void Portal::end() {
    if (!active_)
        return;
    if (req_.upload)
        sink_.abort();
    req_.upload = false;
    wifi_.end();
    progress_.active = false;
    active_ = false;
}

bool Portal::doneRequested() const {
    return stop_ || (done_ && millis() - doneAt_ >= kDoneGraceMs);
}

// Ate kChunk nomes depois de `after`, em ordem. Como o diretorio nao vem ordenado e a RAM e curta,
// a lista sai em blocos: cada passada guarda os menores que ainda nao foram entregues.
uint8_t Portal::listChunk(const char* dir, bool dirs, const char* after) {
    File d = fs_.open(dir);
    if (!d || !d.isDirectory())
        return 0;
    uint8_t n = 0;
    for (File e = d.openNextFile(); e; e = d.openNextFile()) {
        const char* name = e.name();
        const bool valid = e.isDirectory() == dirs && (dirs ? mrm::validPlaylistName(name) : mrm::validTrackName(name));
        if (!valid || order(name, after) <= 0)
            continue;
        uint8_t at = n;
        while (at > 0 && order(chunk_[at - 1], name) > 0)
            --at;
        if (at >= kChunk)
            continue;
        const uint8_t move = n < kChunk ? n - at : kChunk - 1 - at;
        memmove(chunk_[at + 1], chunk_[at], move * sizeof(chunk_[0]));
        memmove(&chunkSize_[at + 1], &chunkSize_[at], move * sizeof(chunkSize_[0]));
        strlcpy(chunk_[at], name, sizeof(chunk_[0]));
        chunkSize_[at] = static_cast<uint32_t>(e.size());
        if (n < kChunk)
            ++n;
    }
    return n;
}

// `after` guarda o ultimo nome entregue; comece com "".
bool Portal::nextChunk(const char* dir, bool dirs, char* after, uint8_t& count) {
    count = listChunk(dir, dirs, after);
    if (count)
        strlcpy(after, chunk_[count - 1], kTrackBuf);
    return count > 0;
}

// Conta as entradas validas (playlists ou faixas) e soma os bytes das faixas.
uint16_t Portal::scanDir(const char* path, bool dirs, uint32_t* bytes) {
    uint16_t count = 0;
    if (bytes)
        *bytes = 0;
    File d = fs_.open(path);
    if (!d || !d.isDirectory())
        return 0;
    for (File e = d.openNextFile(); e; e = d.openNextFile()) {
        const bool valid = e.isDirectory() == dirs && (dirs ? mrm::validPlaylistName(e.name()) : mrm::validTrackName(e.name()));
        if (!valid)
            continue;
        ++count;
        if (bytes)
            *bytes += static_cast<uint32_t>(e.size());
    }
    return count;
}

// Apaga os arquivos da pasta e a pasta. Reabre a cada arquivo para nao mexer na pasta enquanto a lista anda.
bool Portal::purge(const char* path) {
    char file[kPathMax];
    for (;;) {
        File d = fs_.open(path);
        File e = d ? d.openNextFile() : File();
        if (!e)
            break;
        const bool isDir = e.isDirectory();
        snprintf(file, sizeof(file), "%s/%s", path, e.name());
        e.close();
        d.close();
        if (isDir || !fs_.remove(file))
            return false;
    }
    return fs_.rmdir(path);
}

// Playlist vazia (queda de energia logo depois do mkdir de um envio) ocuparia vaga para sempre.
void Portal::sweepEmpty() {
    char path[kPathMax];
    for (bool again = true; again;) {
        again = false;
        File root = fs_.open(kMusicDir);
        for (File e = root ? root.openNextFile() : File(); e; e = root.openNextFile()) {
            if (!e.isDirectory() || !mrm::validPlaylistName(e.name()))
                continue;
            snprintf(path, sizeof(path), "%s/%s", kMusicDir, e.name());
            File d = fs_.open(path);
            const bool empty = d && !d.openNextFile();
            d.close();
            if (!empty)
                continue;
            e.close();
            root.close();
            again = fs_.rmdir(path);
            break;
        }
    }
}

// Troca interrompida (queda entre os renames) deixa ".NN - titulo.mp3": devolve o nome sem o ponto.
bool Portal::restoreSwap(const char* dir) {
    File d = fs_.open(dir);
    for (File e = d ? d.openNextFile() : File(); e; e = d.openNextFile()) {
        const char* name = e.name();
        if (name[0] != '.' || !mrm::validTrackName(name + 1))
            continue;
        char from[kPathMax], to[kPathMax];
        snprintf(from, sizeof(from), "%s/%s", dir, name);
        snprintf(to, sizeof(to), "%s/%s", dir, name + 1);
        if (fs_.exists(to))
            continue;
        e.close();
        d.close();
        return fs_.rename(from, to);
    }
    return false;
}

void Portal::recoverSwap() {
    char dir[kPathMax];
    for (bool again = true; again;) {
        again = false;
        File root = fs_.open(kMusicDir);
        for (File e = root ? root.openNextFile() : File(); e && !again; e = root.openNextFile()) {
            if (!e.isDirectory() || !mrm::validPlaylistName(e.name()))
                continue;
            snprintf(dir, sizeof(dir), "%s/%s", kMusicDir, e.name());
            again = restoreSwap(dir);
        }
    }
}
