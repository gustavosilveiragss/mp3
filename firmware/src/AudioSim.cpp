#include "Audio.h"

#include "Media.h"

namespace {

constexpr uint32_t kScanMs = 6000;
constexpr uint32_t kConnectMs = 1800;
constexpr uint32_t kGiveUpMs = 5000;     // fone fora de alcance
constexpr uint32_t kBytesPerSec = 16000; // MP3 de 128 kbps
constexpr uint32_t kMinSimulatedMs = 20000;

struct Fake {
    const char* name;
    int8_t rssi;
    uint32_t appearsAt;
};
constexpr Fake kFakes[] = {
    {"AirPods Pro", -58, 1000},
    {"Mi Buds", -64, 2200},
    {"JBL Tune 510", -71, 3500},
    {"Fone Casa", -80, 5000},
};

bool isFake(const char* name) {
    for (const Fake& f : kFakes)
        if (strcmp(f.name, name) == 0)
            return true;
    return false;
}

} // namespace

void Audio::begin() {
    link_ = Link::Off; // o radio so liga quando o Settings pedir (setEnabled)
}

void Audio::setEnabled(bool on) {
    if (on == (link_ != Link::Off))
        return;
    scanning_ = false;
    deviceCount_ = 0;
    peer_[0] = '\0';
    state_ = State::Idle;
    link_ = on ? Link::Idle : Link::Off;
}

void Audio::scan() {
    if (link_ == Link::Off)
        return;
    deviceCount_ = 0;
    scanning_ = true;
    scanAt_ = millis();
}

void Audio::connect(const char* name, const uint8_t*) {
    if (link_ == Link::Off)
        return;
    strlcpy(peer_, name, sizeof(peer_));
    link_ = Link::Connecting;
    connectAt_ = millis();
}

void Audio::disconnect() {
    if (link_ == Link::Off)
        return;
    link_ = Link::Idle;
    state_ = State::Idle;
    peer_[0] = '\0';
}

void Audio::forgetAll() {
    disconnect();
}

void Audio::update() {
    const uint32_t now = millis();
    if (scanning_) {
        const uint32_t elapsed = now - scanAt_;
        while (deviceCount_ < sizeof(kFakes) / sizeof(kFakes[0]) && elapsed >= kFakes[deviceCount_].appearsAt) {
            Device& d = devices_[deviceCount_];
            strlcpy(d.name, kFakes[deviceCount_].name, sizeof(d.name));
            d.rssi = kFakes[deviceCount_].rssi;
            ++deviceCount_;
        }
        scanning_ = elapsed < kScanMs;
    }
    if (link_ == Link::Connecting) {
        const uint32_t elapsed = now - connectAt_;
        if (isFake(peer_) && elapsed >= kConnectMs)
            link_ = Link::Connected;
        else if (elapsed >= kGiveUpMs)
            disconnect();
    }
    if (state_ == State::Playing && positionMs() >= durationMs_)
        state_ = State::Finished;
}

bool Audio::play(const char* path) {
    state_ = State::Idle; // consome o Finished: se a faixa nao abrir, o Player nao entra em laco de saltos
    if (!connected())
        return false;
    File file = media::fs().open(path, "r");
    if (!file)
        return false;
    durationMs_ = max<uint32_t>(file.size() / kBytesPerSec * 1000, kMinSimulatedMs);
    file.close();
    basePos_ = 0;
    startedAt_ = millis();
    state_ = State::Playing;
    return true;
}

void Audio::pause() {
    if (state_ != State::Playing)
        return;
    basePos_ = positionMs();
    state_ = State::Paused;
}

void Audio::resume() {
    if (state_ != State::Paused)
        return;
    startedAt_ = millis();
    state_ = State::Playing;
}

uint32_t Audio::positionMs() const {
    return state_ == State::Playing ? basePos_ + (millis() - startedAt_) : basePos_;
}
