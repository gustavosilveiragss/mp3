#pragma once

#include <Arduino.h>

// Saida de audio: acha e conecta fones Bluetooth e toca MP3 por A2DP. Audio.cpp usa o Bluetooth real
// (mrm::BtSource) e o decoder real (mrm::Mp3Stream, so 44,1 kHz). AudioSim.cpp (env mp3sim) simula
// tudo, inclusive fones falsos, para testar a UI sem fone.
class Audio {
public:
    static constexpr uint8_t kMaxDevices = 8;

    struct Device {
        char name[33];
        int8_t rssi; // dBm
    };
    enum class Link : uint8_t { Off,
                                Idle,
                                Connecting,
                                Connected };
    enum class State : uint8_t { Idle,
                                 Playing,
                                 Paused,
                                 Finished };

    void begin();
    void update();

    void setEnabled(bool on); // liga ou desliga o radio; desligar derruba fone e musica
    void scan();              // zera a lista e procura de novo, sem bloquear
    // Procura o nome na varredura; sem achar, usa bda (o guardado em Settings) se vier.
    void connect(const char* name, const uint8_t* bda = nullptr);
    void disconnect();
    void forgetAll(); // apaga todos os pareamentos guardados no radio

    Link link() const { return link_; }
    bool connected() const { return link_ == Link::Connected; }
    bool scanning() const { return scanning_; }
    const char* peer() const { return peer_; } // fone conectado ou em conexao
    const uint8_t* peerBda() const {           // nullptr se nao ha BDA (simulado ou sem conexao)
        for (uint8_t b : peerBda_)
            if (b)
                return peerBda_;
        return nullptr;
    }
    const Device* devices() const { return devices_; }
    uint8_t deviceCount() const { return deviceCount_; }

    bool play(const char* path); // false se sem fone ou arquivo ilegivel
    void pause();
    void resume();
    void setVolume(uint8_t percent) { volume_ = percent; }

    State state() const { return state_; }
    uint32_t positionMs() const;
    uint32_t durationMs() const { return durationMs_; }

private:
    void stopTrack();
    void repairBond(uint32_t now); // so o real
    void readDevices();

    Link link_ = Link::Off;
    State state_ = State::Idle;
    bool scanning_ = false;
    uint8_t volume_ = 0;
    char peer_[33] = "";
    uint8_t peerBda_[6]{};
    Device devices_[kMaxDevices]{};
    uint8_t deviceCount_ = 0;

    uint32_t scanAt_ = 0;
    uint32_t connectAt_ = 0;
    uint32_t switchAt_ = 0; // so o real: quando o link anterior caiu e a troca de fone pode seguir
    bool switching_ = false;
    uint32_t dialAt_ = 0;    // so o real: quando o ultimo connect comecou
    bool repaired_ = false;  // so o real: ja apagamos o pareamento nesta tentativa
    uint32_t startedAt_ = 0; // so o simulado: millis do inicio da retomada atual
    uint32_t basePos_ = 0;   // so o simulado: posicao acumulada antes da retomada atual
    uint32_t durationMs_ = 0;
    uint8_t appliedVolume_ = 0xFF; // so o real: ultimo volume passado ao Bluetooth
};
