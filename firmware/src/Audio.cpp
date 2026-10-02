#include "Audio.h"

#include <BtSource.h>
#include <Mp3Probe.h>
#include <Mp3Stream.h>

#include "Media.h"

#include <fcntl.h>
#include <unistd.h>

#ifndef MP3_BT_DEBUG
#define MP3_BT_DEBUG 0
#endif
#ifndef MP3_BT_CYCLES
#define MP3_BT_CYCLES 0
#endif
#ifndef MP3_BT_CONNTEST
#define MP3_BT_CONNTEST 0
#endif
#ifndef MP3_TEST_DECODE
#define MP3_TEST_DECODE 0 // 1: no boot, antes do radio, mede a banda do decoder com um consumidor falso
#endif
#ifndef MP3_AUDIO_DEBUG
#define MP3_AUDIO_DEBUG 0 // 1: uma linha por segundo e eventos do audio no serial (engasgo com o fone)
#endif
#if MP3_BT_DEBUG || MP3_BT_CYCLES || MP3_BT_CONNTEST || MP3_TEST_DECODE || MP3_AUDIO_DEBUG
#include <Log.h>
#endif
#if MP3_AUDIO_DEBUG
#define AUD_EV(...) mrm::log("aud ", millis(), " ", __VA_ARGS__)
#else
#define AUD_EV(...) ((void)0)
#endif

#if MP3_AUDIO_DEBUG
extern uint8_t gSbcBitpool; // BtWrap.cpp
extern uint16_t gSbcMtu;
#endif

namespace {

constexpr uint32_t kSwitchGapMs = 800; // o fone anterior precisa soltar o link antes do proximo connect
constexpr uint8_t kBtMaxVolume = 127;

mrm::BtSource bt;
mrm::Mp3Stream stream;

#if MP3_AUDIO_DEBUG
// Escritos so pelo callback do A2DP; o loop le e zera uma vez por segundo.
std::atomic<uint32_t> cbCalls{0}, cbBytes{0}, cbMaxGapUs{0}, cbMaxLen{0}, ringMin{UINT32_MAX};
uint32_t cbLastUs = 0;
// Frames por rodada do alarme de 30 ms da pilha: 10-11 e o normal; 7 fixo e a fila do A2DP travada
// em 1 pacote por rodada (engasgo com o fone clone, 2/3 do audio).
std::atomic<uint32_t> tickN{0}, tickMin{UINT32_MAX}, tickMax{0};
uint32_t tickCur = 0;

void closeTick() {
    tickN.fetch_add(1);
    if (tickCur < tickMin.load())
        tickMin.store(tickCur);
    if (tickCur > tickMax.load())
        tickMax.store(tickCur);
    tickCur = 0;
}

void watchFeed(int32_t len) {
    const uint32_t now = micros();
    if (cbLastUs && now - cbLastUs > cbMaxGapUs.load())
        cbMaxGapUs.store(now - cbLastUs);
    if (cbLastUs && now - cbLastUs > 8000 && tickCur)
        closeTick();
    if (len > 0)
        ++tickCur;
    cbLastUs = now;
    if (len <= 0)
        return;
    cbCalls.fetch_add(1);
    cbBytes.fetch_add(len);
    if (uint32_t(len) > cbMaxLen.load())
        cbMaxLen.store(len);
    if (stream.buffered() < ringMin.load())
        ringMin.store(stream.buffered());
}

void report(int link, int state, uint8_t volume) {
    static uint32_t at = 0, under0 = 0, frames0 = 0, busy0 = 0;
    const uint32_t now = millis();
    if (now - at < 1000)
        return;
    at = now;
    const mrm::Mp3Stream::Stats& st = stream.stats();
    const uint32_t under = stream.underruns(), frames = st.frames - frames0;
    mrm::log("aud ", now, " 1s link=", link, " bt=", static_cast<int>(bt.state()), " st=", state, " und=", under, " +",
             under - under0, " ring_min=", ringMin.exchange(UINT32_MAX), " ring=", stream.buffered(),
             " cb=", cbCalls.exchange(0), " B=", cbBytes.exchange(0), " gap_max_us=", cbMaxGapUs.exchange(0),
             " len_max=", cbMaxLen.exchange(0), " dec_fr=", frames, " dec_avg_us=", frames ? (st.busyUs - busy0) / frames : 0,
             " dec_max_us=", st.maxUs, " err=", st.errors, " pilha=", stream.stackFree(), " heap=", ESP.getFreeHeap(),
             " min=", ESP.getMinFreeHeap(), " vol=", volume, " btvol=", bt.get_volume(), " pos=", stream.playedFrames(), " ticks=", tickN.exchange(0),
             " tmin=", tickMin.exchange(UINT32_MAX), " tmax=", tickMax.exchange(0), " bp=", gSbcBitpool, " mtu=", gSbcMtu);
    under0 = under;
    frames0 = st.frames;
    busy0 = st.busyUs;
}
#endif

int32_t feed(uint8_t* data, int32_t len) {
#if MP3_AUDIO_DEBUG
    watchFeed(len);
#endif
    return stream.read(data, len);
}

size_t readAt(void* ctx, uint32_t offset, uint8_t* buf, size_t n) {
    const int fd = *static_cast<int*>(ctx);
    const ssize_t got = lseek(fd, offset, SEEK_SET) == off_t(offset) ? read(fd, buf, n) : -1;
    return got > 0 ? got : 0;
}

// Roda no loop a cada troca de faixa. O File do Arduino faz dois stat antes do fopen (~18 ms no
// LittleFS) e rele 4 KB a cada seek, e o fseek(SEEK_END) do stdio custa ~20 ms; o descritor cru fica
// com o custo do open.
bool probe(const char* path, mrm::Mp3Info& info) {
    char full[mrm::Mp3Stream::kMaxPath + 16];
    snprintf(full, sizeof(full), "%s%s", media::mountPoint(), path);
    int fd = open(full, O_RDONLY);
    if (fd < 0)
        return false;
    const off_t size = lseek(fd, 0, SEEK_END);
    const bool ok = size > 0 && mrm::probeMp3(readAt, &fd, size, info);
    close(fd);
    return ok;
}

#if MP3_TEST_DECODE
// Teste de banda sem fone: uma task no core 0 faz o papel do callback do A2DP (441 frames a cada
// 10 ms, prioridade alta) e o decoder toca o primeiro MP3 de /music em loop por 20 s em cada clock.
std::atomic<bool> consuming{false};

void fakeBt(void*) {
    static uint8_t buf[441 * 4];
    TickType_t last = xTaskGetTickCount();
    while (consuming) {
        stream.read(buf, sizeof(buf));
        vTaskDelayUntil(&last, pdMS_TO_TICKS(10));
    }
    vTaskDelete(nullptr);
}

bool firstMp3In(File dir, char* out, size_t size) {
    for (File f = dir.openNextFile(); f; f = dir.openNextFile())
        if (!f.isDirectory() && strstr(f.name(), ".mp3")) {
            strlcpy(out, f.path(), size);
            return true;
        }
    return false;
}

bool firstTrack(char* out, size_t size) {
    File root = media::fs().open("/music");
    for (File d = root.openNextFile(); d; d = root.openNextFile())
        if (d.isDirectory() && firstMp3In(d, out, size))
            return true;
    return false;
}

void decodeRun(const char* path, const mrm::Mp3Info& info, uint32_t mhz) {
    setCpuFrequencyMhz(mhz);
    const mrm::Mp3Stream::Stats s0 = stream.stats();
    const uint32_t under0 = stream.underruns();
    const uint32_t t0 = millis();
    uint16_t loops = 0;
    stream.start(path, info);
    while (millis() - t0 < 20000) {
        vTaskDelay(pdMS_TO_TICKS(20));
        if (stream.finished() && stream.start(path, info))
            ++loops;
    }
    stream.stop();
    const mrm::Mp3Stream::Stats& s = stream.stats();
    const uint32_t frames = s.frames - s0.frames;
    const uint32_t busy = s.busyUs - s0.busyUs;
    mrm::log("dec ", mhz, " MHz: frames=", frames, " us/frame=", frames ? busy / frames : 0, " max=", s.maxUs,
             " cpu=", busy / ((millis() - t0) * 10), "% underruns=", stream.underruns() - under0,
             " erros=", s.errors - s0.errors, " voltas=", loops, " heap=", ESP.getFreeHeap(),
             " min=", ESP.getMinFreeHeap(), " pilha livre=", stream.stackFree());
}

// Cada faixa por 3 s: start aceita so 44,1 kHz, decodifica sem erro (mono e VBR) e o stop e curto.
void eachTrack() {
    File dir = media::fs().open("/music/teste");
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
        char p[96];
        mrm::Mp3Info info;
        strlcpy(p, f.path(), sizeof(p));
        f.close();
        const mrm::Mp3Stream::Stats s0 = stream.stats();
        const bool started = probe(p, info) && stream.start(p, info);
        vTaskDelay(pdMS_TO_TICKS(3000));
        const uint32_t played = stream.playedFrames();
        const uint32_t t = micros();
        stream.stop();
        mrm::log("dec faixa start=", started, " frames=", stream.stats().frames - s0.frames,
                 " erros=", stream.stats().errors - s0.errors, " tocado=", played * 10 / 441, " ms stop=", micros() - t,
                 " us ", p);
    }
}

void decodeTest(uint32_t heapBefore) {
    char path[96];
    mrm::Mp3Info info;
    if (!firstTrack(path, sizeof(path))) {
        mrm::log("dec sem mp3 em /music");
        return;
    }
    File dir = media::fs().open("/music/teste");
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
        char p[96];
        strlcpy(p, f.path(), sizeof(p));
        f.close();
        const uint32_t t = micros();
        const bool ok = probe(p, info);
        mrm::log("dec probe ", ok, " ", micros() - t, " us dur=", info.durationMs, " rate=", info.sampleRate, " ", p);
    }
    const uint32_t p0 = micros();
    const bool ok = probe(path, info);
    mrm::log("dec ", path, " probe=", ok, " em ", micros() - p0, " us dur=", info.durationMs, " ms off=", info.dataOffset,
             " rate=", info.sampleRate, " kbps=", info.kbps, " ch=", info.channels);
    mrm::log("dec heap antes do begin=", heapBefore, " depois=", ESP.getFreeHeap());
    consuming = true;
    xTaskCreatePinnedToCore(fakeBt, "fakebt", 2048, nullptr, 19, nullptr, 0);
    for (uint32_t mhz : {240u, 160u, 80u})
        decodeRun(path, info, mhz);
    setCpuFrequencyMhz(240);
    eachTrack();
    consuming = false;
    setCpuFrequencyMhz(240);
    vTaskDelay(pdMS_TO_TICKS(50));
    mrm::log("dec fim heap=", ESP.getFreeHeap(), " min=", ESP.getMinFreeHeap());
}
#endif

#if MP3_BT_CONNTEST
// Conecta num BDA que nao existe durante a varredura (cancela, tenta, falha) e varre de novo.
void connectTest(Audio& audio, uint32_t now) {
    static const uint8_t ghost[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
    static uint8_t step = 0;
    if (step == 0 && now > 5000) {
        mrm::log("bt-conntest connect fantasma");
        audio.connect("fantasma", ghost);
        step = 1;
    } else if (step == 1 && now > 24000) {
        mrm::log("bt-conntest link=", static_cast<int>(audio.link()), " scan de novo");
        audio.scan();
        step = 2;
    }
}
#endif

#if MP3_BT_CYCLES
// Ciclos stop/start do radio com o heap no log, para provar que a memoria volta. So em bancada.
void logHeap(const char* tag, uint32_t tookMs) {
    mrm::log("bt-heap ", tag, " free=", ESP.getFreeHeap(), " min=", ESP.getMinFreeHeap(), " took=", tookMs, "ms");
}

void heapCycles(Audio& audio, uint32_t now) {
    static uint32_t nextAt = 45000; // deixa as varreduras e o teste de conexao terminarem antes
    static uint8_t step = 0;
    if (step >= 2 * MP3_BT_CYCLES || now < nextAt)
        return;
    logHeap("antes", 0);
    const bool stopping = step % 2 == 0;
    audio.setEnabled(!stopping);
    logHeap(stopping ? "stop" : "start", millis() - now);
    if (++step == 2 * MP3_BT_CYCLES)
        mrm::log("bt-heap ciclos fim");
    nextAt = millis() + 9000;
}
#endif

} // namespace

void Audio::begin() {
    link_ = Link::Off; // o radio so liga quando o Settings pedir (setEnabled)
#if MP3_TEST_DECODE
    const uint32_t heapBefore = ESP.getFreeHeap();
#endif
    stream.begin(media::fs());
    bt.set_data_callback(feed);
#if MP3_TEST_DECODE
    decodeTest(heapBefore);
#endif
}

void Audio::stopTrack() {
    if (state_ != State::Idle)
        AUD_EV("stop st=", static_cast<int>(state_));
    stream.stop(); // fecha o arquivo antes de o radio cair ou o portal mexer na midia
    state_ = State::Idle;
}

void Audio::setEnabled(bool on) {
    if (on == (link_ != Link::Off))
        return;
    AUD_EV("radio ", on);
    switching_ = false;
    scanning_ = false;
    deviceCount_ = 0;
    peer_[0] = '\0';
    stopTrack();
    if (on) {
        bt.start();
        link_ = bt.state() == mrm::BtSource::State::Off ? Link::Off : Link::Idle;
        scanning_ = bt.state() == mrm::BtSource::State::Scanning;
    } else {
        bt.stop(); // bloqueia ate ~2,3 s se havia fone conectado
        link_ = Link::Off;
    }
}

void Audio::scan() {
    if (link_ == Link::Off)
        return;
    AUD_EV("scan pedido bt=", static_cast<int>(bt.state()));
    if (switching_ || bt.state() == mrm::BtSource::State::Connecting)
        disconnect(); // varredura com tentativa de conexao em curso so atrapalha as duas
    if (!bt.scan())
        return; // com fone conectado a lista fica como esta
    deviceCount_ = 0;
    scanning_ = true;
}

void Audio::connect(const char* name, const uint8_t* bda) {
    if (link_ == Link::Off)
        return;
    const uint8_t* target = nullptr;
    for (uint8_t i = 0; i < bt.foundCount(); ++i)
        if (strcmp(bt.found(i).name, name) == 0)
            target = bt.found(i).bda;
    if (!target)
        target = bda;
    AUD_EV("connect ", name, " bt=", static_cast<int>(bt.state()), " bda=", target || bda);
    if (!target)
        return; // nome fora da varredura e sem BDA guardado: nada a tentar
    using S = mrm::BtSource::State;
    const S s = bt.state();
    memmove(peerBda_, target, sizeof(peerBda_));
    if (s == S::Connecting || s == S::Connected || s == S::Streaming) {
        bt.disconnect();
        switching_ = true;
        switchAt_ = millis();
    } else if (!bt.connect(peerBda_)) {
        return;
    }
    strlcpy(peer_, name, sizeof(peer_));
    stopTrack();
    link_ = Link::Connecting;
}

void Audio::disconnect() {
    if (link_ == Link::Off)
        return;
    AUD_EV("disconnect bt=", static_cast<int>(bt.state()));
    switching_ = false;
    stopTrack();
    bt.disconnect();
    link_ = Link::Idle;
    peer_[0] = '\0';
}

// Lista da varredura para o Picker, copiada no loop porque a pilha escreve na task do BT.
void Audio::readDevices() {
    using S = mrm::BtSource::State;
    const S s = bt.state();
    scanning_ = s == S::Scanning;
    const uint8_t count = min<uint8_t>(bt.foundCount(), kMaxDevices);
    for (uint8_t i = 0; i < count; ++i) {
        strlcpy(devices_[i].name, bt.found(i).name, sizeof(devices_[i].name));
        devices_[i].rssi = bt.found(i).rssi;
    }
#if MP3_AUDIO_DEBUG
    static S lastAud = S::Off;
    if (s != lastAud) {
        AUD_EV("bt ", static_cast<int>(lastAud), "->", static_cast<int>(s), " (2 scan 3 conectando 4 link 5 streaming)");
        lastAud = s;
    }
#endif
#if MP3_BT_DEBUG
    for (uint8_t i = deviceCount_; i < count; ++i)
        mrm::log("bt achado ", devices_[i].name, " rssi=", devices_[i].rssi);
    static S lastBt = S::Off;
    if (s != lastBt) {
        mrm::log("bt estado ", static_cast<int>(s), " heap=", ESP.getFreeHeap(), " min=", ESP.getMinFreeHeap());
        lastBt = s;
    }
#endif
    deviceCount_ = count;
}

void Audio::update() {
    const uint32_t now = millis();
#if MP3_BT_CONNTEST
    connectTest(*this, now);
#endif
#if MP3_BT_CYCLES
    heapCycles(*this, now);
#endif
    if (link_ == Link::Off)
        return;
    bt.update();
    using S = mrm::BtSource::State;
    if (switching_ && now - switchAt_ >= kSwitchGapMs) {
        switching_ = false;
        if (!bt.connect(peerBda_))
            peer_[0] = '\0';
    }

    readDevices();
    const S s = bt.state();
    const Link was = link_;
    if (switching_ || s == S::Connecting)
        link_ = Link::Connecting;
    else if (s == S::Connected || s == S::Streaming)
        link_ = Link::Connected;
    else
        link_ = Link::Idle;
    if (link_ == Link::Idle)
        peer_[0] = '\0';
    if (was == Link::Connected && link_ != Link::Connected)
        stopTrack(); // o fone caiu
    if (volume_ != appliedVolume_) {
        appliedVolume_ = volume_;
        bt.set_volume(uint32_t(volume_) * kBtMaxVolume / 100); // a lib aplica o ganho depois do read()
        AUD_EV("volume ", volume_, " bt=", bt.get_volume());
    }
    if (state_ == State::Playing && stream.finished()) {
        state_ = State::Finished;
        AUD_EV("fim da faixa");
    }
#if MP3_AUDIO_DEBUG
    report(static_cast<int>(link_), static_cast<int>(state_), volume_);
#endif
}

bool Audio::play(const char* path) {
    state_ = State::Idle; // consome o Finished: se a faixa nao abrir, o Player nao entra em laco de saltos
    mrm::Mp3Info info;
    const bool ok = connected() && probe(path, info) && stream.start(path, info);
    AUD_EV("play ", ok, " bt=", static_cast<int>(bt.state()), " ring=", stream.buffered(), " ", path);
    if (!ok)
        return false; // ilegivel, nao e MP3 ou taxa diferente de 44,1 kHz
    durationMs_ = info.durationMs;
    state_ = State::Playing;
    return true;
}

void Audio::pause() {
    if (state_ != State::Playing)
        return;
    stream.pause();
    state_ = State::Paused;
    AUD_EV("pause");
}

void Audio::resume() {
    if (state_ != State::Paused)
        return;
    stream.resume();
    state_ = State::Playing;
    AUD_EV("resume");
}

uint32_t Audio::positionMs() const {
    const uint32_t ms = uint64_t(stream.playedFrames()) * 1000 / mrm::Mp3Stream::kRate;
    return min(ms, durationMs_);
}
