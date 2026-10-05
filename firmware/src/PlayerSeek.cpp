#include "Player.h"

#include "Seek.h"

// Modo busca: A anda para trás e B para frente (toque 10 s, segurar 30 s por repetição).
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
        seek_.active = false;
        break;

    default:
        break;
    }
}

void Player::seekBy(int direction, bool hold) {
    const uint32_t now = millis();
    const uint32_t step = hold ? seek::kHoldMs : seek::kTapMs;

    if (!seek_.dirty)
        seek_.targetMs = positionMs(); // a música andou desde o último destino

    seek_.targetMs = seek::clampMs(int64_t(seek_.targetMs) + direction * int64_t(step), durationMs());

    seek_.dirty = true;
    seek_.lastKeyAt = now;
}

// Sem faixa carregada (pausada desde o boot ou sem fone) a duração vem do arquivo, uma vez por faixa.
uint32_t Player::durationMs() {
    if (loaded_)
        return audio_.durationMs();

    char path[dirlist::kPath];
    if (resume_.idleDurationMs == 0 && library_.trackPath(trackName_, path, sizeof(path)))
        resume_.idleDurationMs = audio_.durationOf(path);

    return resume_.idleDurationMs;
}

void Player::commitSeek() {
    if (!seek_.dirty)
        return;

    if (loaded_ && !audio_.seek(seek_.targetMs)) {
        seek_.lastKeyAt = millis(); // fila do stream cheia: o destino fica e tenta de novo
        return;
    }

    seek_.dirty = false;

    if (!loaded_)
        resume_.startMs = seek_.targetMs; // sem faixa carregada (sem fone): vale quando ela carregar

    resume_.lastPosMs = seek_.targetMs;
    savedAt_ = millis();
    remember();
}

void Player::updateSeek(uint32_t now) {
    if (!seek_.active)
        return;

    if (seek_.dirty && now - seek_.lastKeyAt >= seek::kCommitDelayMs)
        commitSeek();

    if (now - seek_.lastKeyAt >= seek::kModeIdleMs) {
        commitSeek();
        seek_.active = false;
    }
}
