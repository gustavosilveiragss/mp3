#pragma once

#include <Arduino.h>
#include <FS.h>
#include <JsonOut.h>
#include <Mp3Sniff.h>
#include <UploadSink.h>
#include <WifiPortal.h>
#include <marmota.h>

#include "Library.h"
#include "PortalDebug.h"
#include "PortalErrors.h"
#include "PortalPage.h"
#include "PortalSwap.h"

static_assert(HTTP_RAW_BUFLEN <= HTTP_UPLOAD_BUFLEN,
              "HTTPRaw::buf tem o tamanho de HTTP_UPLOAD_BUFLEN: os dois -D andam juntos");

inline constexpr char kTmpPath[] = "/music/.up.tmp"; // sobra de upload interrompido, apagada no begin

// Portal Wi-Fi para enviar, apagar, renomear, mover e reordenar músicas em /music (e achar os .mp3
// soltos que já estavam na mídia). A página vive na flash. As rotas
// falam JSON (contrato em firmware/web/mock/server.js). handle() bloqueia durante um arquivo: o tick
// roda a cada bloco recebido (~10 ms) lá dentro, para o loop ler as teclas sem perder toque curto ou
// A+B. Quem desenha limita a própria taxa. A parada nunca roda no tick: requestStop() só marca, e
// quem chamou handle() para depois que ele volta.
class Portal {
public:
    static constexpr uint32_t kIdleStopMs = 10UL * 60 * 1000; // sem requisição (celular associado e parado não conta)
    static constexpr uint32_t kDoneGraceMs = 400;             // deixa /api/done responder antes de derrubar a rede
    static constexpr uint32_t kReserveBytes = 64 * 1024;
    static constexpr uint32_t kMaxFileBytes = 1UL << 30; // 1 GiB: um podcast de horas cabe. FAT32 vai a 4 GiB
    static constexpr uint8_t kMaxLoose = 64;             // .mp3 soltos listados em /api/loose

    struct Progress {
        bool active = false; // um arquivo está chegando
        bool more = false;   // acabou um arquivo e a fila ainda tem o próximo: a tela não pisca entre eles
        uint32_t at = 0;     // último byte ou fim de arquivo
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
    bool doneRequested() const; // A+B, "terminar" na página ou 10 min sem requisição
    bool changed() const { return changed_; }
    bool clientConnected() const { return active_ && clients_ > 0; }
    const Progress& progress() const { return progress_; }

private:
    static constexpr uint8_t kChunk = 16;
    static constexpr size_t kPlaylistBuf = mrm::kMaxPlaylistLen + 1;
    static constexpr size_t kTrackBuf = mrm::kMaxTrackLen + 1;
    static constexpr size_t kPathMax = dirlist::kPath;
    static constexpr size_t kQueryMax = 512;
    static constexpr uint32_t kMaxActionBody = 256;
    static constexpr uint32_t kStallMs = 12000;     // upload sem um byte por esse tempo e abandonado
    static constexpr uint32_t kRecvTimeoutS = 5;    // pausa do celular sem abortar o upload
    static constexpr uint32_t kTailTimeoutMs = 750; // fim do corpo: o core espera encher HTTP_RAW_BUFLEN ou o timeout

    static constexpr size_t kErrorBodyMax = 32;
    static constexpr size_t kOkBodyMax = 48;
    static constexpr size_t kUploadBodyMax = 64;
    static constexpr size_t kLooseSrcMax = 8;
    static constexpr uint32_t kMaxQueueFiles = UINT16_MAX;
    static constexpr uint32_t kClientsPollMs = 500;

    struct Query {
        char text[kQueryMax]; // X-Q do pedido
        char playlist[kPlaylistBuf];
        char track[kTrackBuf];
        char target[kTrackBuf];
    };

    struct UploadState {
        uint32_t length;
        uint32_t base; // bytes dos arquivos anteriores da fila
        bool started;  // startRaw rodou neste pedido: sem isso o corpo não passou pelas checagens
        bool replied;
        bool open; // há arquivo sendo gravado
        bool newDir;
    };

    // PortalRoutes.cpp
    void registerRoutes(WebServer& server);
    void onRaw();
    void startRaw();
    void readUploadToken();
    bool readQuery();
    bool claim();
    void notFound();
    void doDone();
    void sendJson(int code, const char* body);
    void sendError(int code, const char* error);
    void sendOk(bool withFree);

    // PortalUpload.cpp
    void armTimeout(uint32_t remaining);
    void startUpload();
    void readProgress();
    void writeRaw(const HTTPRaw& raw);
    void finishUpload();
    void reject(int code, const char* error);
    void rejectSink(mrm::UploadSink::Error error);
    void cancelUpload();
    void abandonUpload(const char* reason, bool closeSocket);
    void closeConnection();
    void dropNewDir();
    void clearProgress() { progress_.active = progress_.more = false; }

    // PortalLibrary.cpp
    void beginJsonStream();
    void sendState();
    void sendTracks();
    void sendLoose();
    void sendLog();
    void writePlaylists(mrm::JsonOut& json);
    void doDelete();
    void doRename();
    void renameCase(const char* src, const char* dst);
    bool looseBase(char* base, size_t cap);
    int resolve(char* path, size_t cap, bool& track);

    // PortalManage.cpp
    void doMove();
    void doSwap();
    bool targetFull(const char* dir, bool isNew);
    void removeSourceIfEmpty();
    bool trackExists(const char* playlist, const char* track);
    bool readSwapQuery(trackswap::SwapNames& names);
    bool swapFiles(const trackswap::SwapNames& names);

    // Portal.cpp
    void touch() { idleAt_ = millis(); }
    uint8_t listChunk(const char* dir, bool dirs, const char* after);
    bool nextChunk(const char* dir, bool dirs, char* after, uint8_t& count);
    bool purge(const char* path);
    void sweepEmpty();
    void recoverSwap();
    bool findSwap(const char* dir, char* from, char* to, size_t size);

    fs::FS& fs_;
    const mrm::Locale& locale_;
    char ssid_[33] = "";
    mrm::WifiPortal wifi_;
    mrm::Mp3Sniff sniff_;
    mrm::UploadSink sink_;
    WebServer* server_ = nullptr;

    Progress progress_;
    Query query_{};
    UploadState upload_{};
    char chunk_[kChunk][kTrackBuf];
    uint32_t chunkSize_[kChunk];
    char last_[portalhttp::kLastMax] = "ok";
    uint32_t lastU_ = 0; // token do envio a que last_ se refere

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
