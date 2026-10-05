#include "Keypad.h"

namespace {

constexpr uint32_t kDebounceMs = 30;
constexpr uint32_t kHoldMs = 400;
constexpr uint32_t kRepeatMs = 150;
constexpr uint32_t kDoubleGapMs = 250;
constexpr uint32_t kChordWindowMs = 200; // rolar o dedo de uma tecla a outra tem de cair em combinação, não em volume
constexpr uint32_t kChordHoldMs = 1500;
constexpr uint32_t kChordReleaseMs = 300; // as duas teclas soltas quase juntas

} // namespace

void Keypad::begin() {
    pinMode(a_.pin, INPUT_PULLUP);
    pinMode(b_.pin, INPUT_PULLUP);
}

bool Keypad::debounce(Key& key, uint32_t now) {
    const bool level = digitalRead(key.pin) == LOW;
    if (level != key.raw) {
        key.raw = level;
        key.rawAt = now;
    }

    if (now - key.rawAt < kDebounceMs || key.down == key.raw)
        return false;

    key.down = key.raw;
    (key.down ? key.downAt : key.releasedAt) = now;
    return true;
}

void Keypad::push(Action action) {
    if (count_ < 4)
        queue_[(head_ + count_++) % 4] = action;
}

Keypad::Action Keypad::update() {
    const uint32_t now = millis();
    const bool aChanged = debounce(a_, now);
    const bool bChanged = debounce(b_, now);

    const bool pressed = (aChanged && a_.down) || (bChanged && b_.down);
    if (mode_ == Mode::Idle && pressed && a_.down && b_.down && !a_.held && !b_.held &&
        (a_.downAt > b_.downAt ? a_.downAt - b_.downAt : b_.downAt - a_.downAt) <= kChordWindowMs) {
        mode_ = Mode::Chord;
        chordAt_ = now;
        chordFired_ = false;
        chordBroken_ = false;

        a_.taps = b_.taps = 0;
    }

    if (mode_ == Mode::Idle)
        updateIdle(now, aChanged, bChanged);
    else
        updateChord(now);

    if (count_ == 0)
        return Action::None;

    const Action out = queue_[head_];
    head_ = (head_ + 1) % 4;
    --count_;

    return out;
}

void Keypad::updateIdle(uint32_t now, bool aChanged, bool bChanged) {
    for (Key* key : {&a_, &b_}) {
        const bool isA = key == &a_;
        const bool changed = isA ? aChanged : bChanged;

        if (key->down && !key->held && now - key->downAt >= kHoldMs) {
            key->held = true;
            key->taps = 0;
            key->repeatAt = now;
        }

        if (key->held && key->down && static_cast<int32_t>(now - key->repeatAt) >= 0) {
            key->repeatAt = now + kRepeatMs;
            push(isA ? Action::VolDown : Action::VolUp);
        }

        if (changed && !key->down) { // lint: ok cadeia única de gestos da tecla
            if (key->held)
                key->held = false;
            else if (instant_)
                push(isA ? Action::PlayPause : Action::Next);
            else
                ++key->taps;
        }
    }

    if (a_.taps > 0 && !a_.down && now - a_.releasedAt >= kDoubleGapMs) {
        push(a_.taps == 1 ? Action::PlayPause : Action::Prev);
        a_.taps = 0;
    }

    if (b_.taps > 0 && !b_.down && now - b_.releasedAt >= kDoubleGapMs) {
        push(b_.taps == 1 ? Action::Seek : Action::Next);
        b_.taps = 0;
    }
}

void Keypad::updateChord(uint32_t now) {
    if (!a_.down || !b_.down) {
        if (!chordBroken_)
            firstUpAt_ = now;
        chordBroken_ = true;
    }

    if (!chordFired_ && !chordBroken_ && now - chordAt_ >= kChordHoldMs) {
        chordFired_ = true;
        push(Action::OpenMenu);
    }

    if (a_.down || b_.down)
        return;

    if (!chordFired_ && now - firstUpAt_ <= kChordReleaseMs)
        push(Action::NextPlaylist);

    mode_ = Mode::Idle;
}
