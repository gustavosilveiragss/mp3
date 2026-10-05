#include "Audio.h"

#include <Mp3Probe.h>
#include <Mp3Tag.h>

#include "Media.h"

#include <fcntl.h>
#include <unistd.h>

#include "AudioCore.h"
#include "AudioDebug.h"
#include "Seek.h"

namespace audio {
mrm::BtSource bt;
mrm::Mp3Stream stream;
} // namespace áudio

using audio::bt;
using audio::stream;

namespace {

constexpr uint32_t kSwitchGapMs = 800; // o fone anterior precisa soltar o link antes do próximo connect
constexpr uint8_t kBtMaxVolume = 127;
constexpr uint8_t kPercentMax = 100;

int32_t feed(uint8_t* data, int32_t len) {
    AUD_FEED_START(len);
    const int32_t got = stream.read(data, len);
    AUD_FEED_DONE(data, got);
    return got;
}

size_t readAt(void* ctx, uint32_t offset, uint8_t* buf, size_t n) {
    const int fd = *static_cast<int*>(ctx);
    const ssize_t got = lseek(fd, offset, SEEK_SET) == off_t(offset) ? read(fd, buf, n) : -1;
    return got > 0 ? got : 0;
}

// Roda a cada troca de faixa. O File do Arduino faz dois stat antes do fopen e relé 4 KB a cada seek, e o
// fseek(SEEK_END) do stdio custa ~20 ms. O descritor cru fica só com o custo do open.
bool probe(const char* path, mrm::Mp3Info& info, char* artist) {
    char full[mrm::Mp3Stream::kMaxPath + 16];
    snprintf(full, sizeof(full), "%s%s", media::mountPoint(), path);

    int fd = open(full, O_RDONLY);
    if (fd < 0)
        return false;

    const off_t size = lseek(fd, 0, SEEK_END);
    const bool ok = size > 0 && mrm::probeMp3(readAt, &fd, size, info);
    if (ok && artist)
        mrm::readMp3Artist(readAt, &fd, size, artist);

    close(fd);
    return ok;
}

} // namespace

void Audio::begin() {
    link_ = Link::Off; // o rádio só liga quando o Settings pedir (setEnabled)
    stream.begin(media::fs());
    bt.set_data_callback(feed);
}

void Audio::stopTrack() {
    if (state_ != State::Idle)
        AUD_EV("stop st=", static_cast<int>(state_));

    stream.stop(); // fecha o arquivo antes de o rádio cair ou o portal mexer na mídia
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
        bt.set_ssp_enabled(true); // IDF 5: sem isso o IO cap não é definido e o pareamento do fone falha
        bt.start();

        link_ = bt.state() == mrm::BtSource::State::Off ? Link::Off : Link::Idle;
        scanning_ = bt.state() == mrm::BtSource::State::Scanning;
    } else {
        bt.stop(); // bloqueia até ~2,3 s se havia fone conectado
        link_ = Link::Off;
    }
}

void Audio::scan() {
    if (link_ == Link::Off)
        return;

    AUD_EV("scan pedido bt=", static_cast<int>(bt.state()));

    if (switching_ || bt.state() == mrm::BtSource::State::Connecting)
        disconnect(); // varredura com tentativa de conexão em curso só atrapalha as duas

    if (!bt.scan())
        return; // com fone conectado a lista fica como está

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

    AUD_BT_STATE(s);
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
        bt.set_volume(uint32_t(volume_) * kBtMaxVolume / kPercentMax); // a lib aplica o ganho depois do read()
        AUD_EV("volume ", volume_, " bt=", bt.get_volume());
    }

    if (state_ == State::Playing && stream.finished()) {
        state_ = State::Finished;
        AUD_EV("fim da faixa");
    }

    AUD_REPORT(link_, state_, volume_);
}

bool Audio::play(const char* path, uint32_t startMs) {
    // Fecha a faixa anterior porque o cartão tem um slot de arquivo só e o probe abre outro.
    // O Finished some junto, e assim o Player não entra em laço de saltos se a faixa não abrir.
    stopTrack();

    mrm::Mp3Info info;
    track_.artist[0] = '\0';

    const bool ok = connected() && strlen(path) < sizeof(track_.path) && probe(path, info, track_.artist);
    if (ok) {
        track_.info = info;
        track_.durationMs = info.durationMs;
        strlcpy(track_.path, path, sizeof(track_.path));
        track_.baseMs = seek::clampMs(startMs, track_.durationMs);
        info.dataOffset = mrm::mp3OffsetAt(info, track_.baseMs);
    }

    const bool started = ok && stream.start(path, info);
    AUD_EV("play ", started, " bt=", static_cast<int>(bt.state()), " ring=", stream.buffered(), " ", path);

    if (!started)
        return false; // ilegível, não e MP3 ou taxa que o decoder não converte (só 44,1 e 48 kHz)

    state_ = State::Playing;
    return true;
}

uint32_t Audio::durationOf(const char* path) {
    mrm::Mp3Info info;
    return probe(path, info, nullptr) ? info.durationMs : 0;
}

// Troca só o ponto de leitura: o stream fecha e reabre o arquivo no byte novo e o ring velho é descartado.
bool Audio::seek(uint32_t ms) {
    if (state_ != State::Playing && state_ != State::Paused)
        return false;

    mrm::Mp3Info at = track_.info;
    const uint32_t target = seek::clampMs(ms, track_.durationMs);
    at.dataOffset = mrm::mp3OffsetAt(track_.info, target);

    if (!stream.start(track_.path, at))
        return false;

    track_.baseMs = target;

    if (state_ == State::Paused)
        stream.pause();

    AUD_EV("seek ", target);
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
    const uint32_t ms = track_.baseMs + uint64_t(stream.playedFrames()) * 1000 / mrm::Mp3Stream::kRate;
    return min(ms, track_.durationMs);
}
