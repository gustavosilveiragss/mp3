#pragma once

#include <Arduino.h>

// Duas teclas (pull-up, ativas em LOW) decodificadas em acoes. Mapa de gestos em CONTROLES.md.
class Keypad {
public:
    enum class Action : uint8_t { None,
                                  PlayPause,
                                  Prev,
                                  Next,
                                  VolDown,
                                  VolUp,
                                  NextPlaylist,
                                  OpenMenu,
                                  Seek }; // B toque: entra no modo busca

    Keypad(uint8_t pinA, uint8_t pinB)
        : a_{pinA}
        , b_{pinB} {}

    void begin();
    // Chamar a cada volta do loop. Devolve uma acao por chamada, ou None.
    Action update();
    // Em menus o toque de A sai na hora, sem esperar um toque duplo.
    void setInstant(bool instant) { instant_ = instant; }
    // Estado das teclas ja sem ruido, para quem precisa de apertar e soltar em tempo real (jogos).
    bool aDown() const { return a_.down; }
    bool bDown() const { return b_.down; }

private:
    struct Key {
        uint8_t pin;
        bool raw = false;
        bool down = false;
        bool held = false;
        uint8_t taps = 0;
        uint32_t rawAt = 0;
        uint32_t downAt = 0;
        uint32_t releasedAt = 0;
        uint32_t repeatAt = 0;
    };
    enum class Mode : uint8_t { Idle,
                                Chord };

    static bool debounce(Key& key, uint32_t now);
    void push(Action action);
    void updateIdle(uint32_t now, bool aChanged, bool bChanged);
    void updateChord(uint32_t now);

    Key a_, b_;
    bool instant_ = false;
    Mode mode_ = Mode::Idle;
    uint32_t chordAt_ = 0;
    uint32_t firstUpAt_ = 0;
    bool chordFired_ = false;
    bool chordBroken_ = false;
    Action queue_[4]{};
    uint8_t head_ = 0;
    uint8_t count_ = 0;
};
