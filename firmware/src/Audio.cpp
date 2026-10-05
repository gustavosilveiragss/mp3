#include "Audio.h"

#include <Mp3Probe.h>

#include "Media.h"

#include <fcntl.h>
#include <unistd.h>

#include "AudioCore.h"
#include "AudioDebug.h"

namespace audio {
mrm::BtSource bt;
mrm::Mp3Stream stream;
} // namespace audio

using audio::bt;
using audio::stream;

namespace {

constexpr uint32_t kSwitchGapMs = 800; // o fone anterior precisa soltar o link antes do proximo connect
constexpr uint8_t kBtMaxVolume = 127;

int32_t feed(uint8_t* data, int32_t len) {
#if MP3_AUDIO_DEBUG
    audiodebug::feedStart(len);
#endif
    const int32_t got = stream.read(data, len);
#if MP3_AUDIO_DEBUG
    audiodebug::feedDone(data, got);
#endif
    return got;
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

} // namespace

void Audio::begin() {
    link_ = Link::Off; // o radio so liga quando o Settings pedir (setEnabled)
    stream.begin(media::fs());
    bt.set_data_callback(feed);
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
#if ESP_ARDUINO_VERSION_MAJOR >= 3
        bt.set_ssp_enabled(true); // IDF 5: sem isso o IO cap nao e definido e o pareamento do fone falha
#endif
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
    repaired_ = false;
    dialAt_ = millis();
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
    deviceCount_ = count;
}

void Audio::update() {
    const uint32_t now = millis();
    if (link_ == Link::Off)
        return;
    bt.update();
    using S = mrm::BtSource::State;
    if (switching_ && now - switchAt_ >= kSwitchGapMs) {
        switching_ = false;
        dialAt_ = now;
        if (!bt.connect(peerBda_))
            peer_[0] = '\0';
    }
    repairBond(now);

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
    audiodebug::report(static_cast<int>(link_), static_cast<int>(state_), volume_);
#endif
}

bool Audio::play(const char* path) {
    stopTrack();       // fecha a faixa anterior: o cartao tem um slot de arquivo so, e o probe abre outro
    mrm::Mp3Info info; // (o Finished some junto: se a faixa nao abrir, o Player nao entra em laco de saltos)
    const bool ok = connected() && probe(path, info) && stream.start(path, info);
    AUD_EV("play ", ok, " bt=", static_cast<int>(bt.state()), " ring=", stream.buffered(), " ", path);
    if (!ok)
        return false; // ilegivel, nao e MP3 ou taxa que o decoder nao converte (so 44,1 e 48 kHz)
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
