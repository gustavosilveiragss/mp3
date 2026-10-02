#pragma once

#include <Arduino.h>
#include <FS.h>
#include <JsonOut.h>
#include <Mp3Sniff.h>
#include <UploadSink.h>
#include <WifiPortal.h>
#include <marmota.h>

#include "Library.h"
#include "PortalPage.h"

inline constexpr char kTmpPath[] = "/music/.up.tmp"; // sobra de upload interrompido, apagada no begin

// Portal Wi-Fi para enviar, apagar, renomear, mover e reordenar musicas em /music (e achar os .mp3
// soltos que ja estavam na midia). A pagina vive na flash; as rotas
// falam JSON (contrato em firmware/web/mock/server.js). handle() bloqueia durante um arquivo: o tick
// roda a cada bloco recebido (~10 ms) la dentro, para o loop ler as teclas sem perder toque curto ou
// A+B; quem desenha limita a propria taxa. A parada nunca roda no tick: requestStop() so marca, e
// quem chamou handle() para depois que ele volta.
class Portal {
public:
    static constexpr uint32_t kIdleStopMs = 10UL * 60 * 1000; // sem nenhuma requisicao (celular associado e parado nao conta)
    static constexpr uint32_t kDoneGraceMs = 400;             // deixa a resposta de /api/done sair antes de derrubar a rede
    static constexpr uint32_t kReserveBytes = 64 * 1024;
    static constexpr uint32_t kMaxFileBytes = 16UL * 1024 * 1024;
    static constexpr uint8_t kMaxLoose = 64; // .mp3 soltos listados em /api/loose

    struct Progress {
        bool active = false; // um arquivo esta chegando
        uint16_t file = 0, files = 0;
        uint32_t sent = 0, size = 0;
        uint32_t totalSent = 0, totalSize = 0;
        char name[mrm::kMaxTrackLen + 1] = "";
    };
    using TickFn = void (*)(void*);

    Portal(fs::FS& fs, const mrm::Locale& locale);

    void begin(const char* ssid);
    void handle();
    void end();
    void onTick(TickFn fn, void* ctx) {
        tick_ = fn;
        tickCtx_ = ctx;
    }

    void requestStop() { stop_ = true; }
    bool doneRequested() const; // A+B, "terminar" na pagina ou 10 min sem requisicao
    bool changed() const { return changed_; }
    bool clientConnected() const { return active_ && clients_ > 0; }
    const Progress& progress() const { return progress_; }

private:
    static constexpr uint8_t kChunk = 16;
    static constexpr size_t kPlaylistBuf = mrm::kMaxPlaylistLen + 1;
    static constexpr size_t kTrackBuf = mrm::kMaxTrackLen + 1;
    static constexpr size_t kPathMax = 96;
    static constexpr size_t kDirMax = sizeof(kMusicDir) + mrm::kMaxPlaylistLen + 1; // /music/<playlist>
    static constexpr size_t kQueryMax = 512;
    static constexpr uint32_t kMaxActionBody = 256;
    static constexpr uint32_t kRecvTimeoutS = 5;    // pausa do celular sem abortar o upload
    static constexpr uint32_t kTailTimeoutMs = 750; // fim do corpo: o core espera encher 1436 B ou estourar o timeout; folga para o RTO do celular

    struct Request {
        char query[kQueryMax];
        char playlist[kPlaylistBuf];
        char track[kTrackBuf];
        char target[kTrackBuf];
        uint32_t length;
        uint32_t base; // bytes dos arquivos anteriores da fila
        bool started;  // startRaw rodou neste pedido: sem isso o corpo nao passou pelas checagens
        bool replied;
        bool upload;
        bool newDir;
    };

    // PortalRoutes.cpp
    void registerRoutes(WebServer& server);
    void sendState();
    void sendTracks();
    void onRaw();
    void startRaw();
    bool claim();
    void armTimeout(uint32_t remaining);
    void startUpload();
    void writeRaw(const HTTPRaw& raw);
    void finishUpload();
    void doDelete();
    void doRename();
    void doMove();
    void doSwap();
    bool swapFiles(const char* dir, const char* a, const char* b, const char* na, const char* nb);
    void sendLoose();
    bool looseBase(char* base, size_t cap);
    void doDone();
    void notFound();
    void reject(int code, const char* error);
    void rejectSink(mrm::UploadSink::Error error);
    void cancelUpload();
    void dropNewDir();
    void sendJson(int code, const char* body);
    void sendError(int code, const char* error);
    void sendOk(bool withFree);
    bool readQuery();
    int resolve(char* path, size_t cap, bool& track);
    void readProgress();
    void writePlaylists(mrm::JsonOut& json);

    // Portal.cpp
    void touch() { idleAt_ = millis(); }
    uint8_t listChunk(const char* dir, bool dirs, const char* after);
    bool nextChunk(const char* dir, bool dirs, char* after, uint8_t& count);
    uint16_t scanDir(const char* path, bool dirs, uint32_t* bytes);
    bool purge(const char* path);
    void sweepEmpty();
    void recoverSwap();
    bool restoreSwap(const char* dir);

    fs::FS& fs_;
    const mrm::Locale& locale_;
    char ssid_[33] = "";
    mrm::WifiPortal wifi_;
    mrm::Mp3Sniff sniff_;
    mrm::UploadSink sink_;
    WebServer* server_ = nullptr;

    Progress progress_;
    Request req_{};
    char chunk_[kChunk][kTrackBuf];
    uint32_t chunkSize_[kChunk];
    char last_[8] = "ok";

    TickFn tick_ = nullptr;
    void* tickCtx_ = nullptr;
    bool active_ = false;
    bool changed_ = false;
    bool stop_ = false;
    bool done_ = false;
    uint8_t clients_ = 0;
    uint32_t doneAt_ = 0;
    uint32_t idleAt_ = 0;
    uint32_t clientsAt_ = 0;
};
