#include "Player.h"

#include "Seek.h"

namespace {
constexpr uint32_t kCommitMs = 350; // sem tecla por isso, o destino e aplicado (segurar vai somando sem reabrir o arquivo)
constexpr uint32_t kModeMs = 4000;  // sem tecla por isso, o modo busca fecha
} // namespace

// Modo busca: A anda para tras e B para frente (toque 10 s, segurar 30 s por repeticao);
// A+B ou 4 s parado saem.
void Player::handleSeek(Keypad::Action action) {
    using Action = Keypad::Action;
    switch (action) {
    case Action::PlayPause:
        seekBy(-1, false);
        break;
    case Action::Next:
        seekBy(+1, false);
        break;
    case Action::VolDown:
        seekBy(-1, true);
        break;
    case Action::VolUp:
        seekBy(+1, true);
        break;
    case Action::NextPlaylist:
    case Action::Seek:
    case Action::OpenMenu:
        commitSeek();
        seeking_ = false;
        break;
    default:
        break;
    }
}

void Player::seekBy(int direction, bool hold) {
    const uint32_t now = millis();
    const uint32_t step = hold ? seek::kHoldMs : seek::kTapMs;
    if (!seekDirty_)
        seekMs_ = positionMs(); // a musica andou desde o ultimo destino
    seekMs_ = seek::clampMs(int64_t(seekMs_) + direction * int64_t(step), durationMs());
    seekDirty_ = true;
    seekAt_ = now;
}

// Sem faixa carregada (pausada desde o boot ou sem fone) a duracao vem do arquivo, uma vez por faixa.
uint32_t Player::durationMs() {
    if (loaded_)
        return audio_.durationMs();
    char path[dirlist::kPath];
    if (idleDurationMs_ == 0 && library_.trackPath(trackName_, path, sizeof(path)))
        idleDurationMs_ = audio_.durationOf(path);
    return idleDurationMs_;
}

void Player::commitSeek() {
    if (!seekDirty_)
        return;
    if (loaded_ && !audio_.seek(seekMs_)) {
        seekAt_ = millis(); // fila do stream cheia: o destino fica e tenta de novo
        return;
    }
    seekDirty_ = false;
    if (!loaded_)
        startMs_ = seekMs_; // sem faixa carregada (sem fone): vale quando ela carregar
    lastPosMs_ = seekMs_;
    savedAt_ = millis();
    remember();
}

void Player::updateSeek(uint32_t now) {
    if (!seeking_)
        return;
    if (seekDirty_ && now - seekAt_ >= kCommitMs)
        commitSeek();
    if (now - seekAt_ >= kModeMs) {
        commitSeek();
        seeking_ = false;
    }
}
