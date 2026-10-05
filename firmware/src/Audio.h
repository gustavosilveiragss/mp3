#pragma once

#include <Arduino.h>
#include <BtSource.h>
#include <Mp3Probe.h>
#include <Mp3Stream.h>
#include <Mp3Tag.h>

// Saída de áudio: acha e conecta fones Bluetooth e toca MP3 por A2DP (mrm::BtSource e mrm::Mp3Stream).
class Audio {
public:
    static constexpr uint8_t kMaxDevices = 8;
    static constexpr uint8_t kVolumeUnset = 0xFF; // forca o primeiro envio de volume

    struct Device {
        char name[mrm::BtSource::kNameLen + 1];
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

    void setEnabled(bool on); // liga ou desliga o rádio, desligar derruba fone e música
    void scan();              // zera a lista e procura de novo, sem bloquear
    // Procura o nome na varredura. Sem achar, usa bda (o guardado em Settings) se vier.
    void connect(const char* name, const uint8_t* bda = nullptr);
    void disconnect();
    void forgetAll(); // apaga todos os pareamentos guardados no rádio

    Link link() const { return link_; }
    bool connected() const { return link_ == Link::Connected; }
    bool scanning() const { return scanning_; }
    const char* peer() const { return peer_; } // fone conectado ou em conexão
    const uint8_t* peerBda() const {           // nullptr se não há BDA sem conexão
        for (uint8_t b : peerBda_)
            if (b)
                return peerBda_;
        return nullptr;
    }
    const Device* devices() const { return devices_; }
    uint8_t deviceCount() const { return deviceCount_; }

    bool play(const char* path, uint32_t startMs = 0); // false se sem fone ou arquivo ilegível
    uint32_t durationOf(const char* path);             // 0 se ilegível, só com a faixa parada (o cartão tem um slot)
    bool seek(uint32_t ms);                            // dentro da faixa atual, tocando ou pausada
    void pause();
    void resume();
    void setVolume(uint8_t percent) { volume_ = percent; }

    State state() const { return state_; }
    uint32_t positionMs() const;
    uint32_t durationMs() const { return track_.durationMs; }
    const char* artist() const { return track_.artist; } // da tag da faixa atual, vazio sem tag

private:
    void stopTrack();
    void repairBond(uint32_t now);
    void readDevices();

    Link link_ = Link::Off;
    State state_ = State::Idle;
    bool scanning_ = false;
    uint8_t volume_ = 0;
    char peer_[mrm::BtSource::kNameLen + 1] = "";
    uint8_t peerBda_[6]{};
    Device devices_[kMaxDevices]{};
    uint8_t deviceCount_ = 0;

    uint32_t switchAt_ = 0; // quando o link anterior caiu e a troca de fone pode seguir
    bool switching_ = false;
    uint32_t dialAt_ = 0;   // quando o último connect começou
    bool repaired_ = false; // já apagamos o pareamento nesta tentativa
    struct CurrentTrack {
        uint32_t durationMs = 0;
        uint32_t baseMs = 0; // instante em que o stream (que conta desde o último start) começou
        mrm::Mp3Info info;
        char path[mrm::Mp3Stream::kMaxPath] = "";
        char artist[mrm::kMaxArtistLen + 1] = "";
    } track_;
    uint8_t appliedVolume_ = kVolumeUnset; // último volume passado ao Bluetooth
};
