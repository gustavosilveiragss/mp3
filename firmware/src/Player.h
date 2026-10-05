#pragma once

#include "Audio.h"
#include "Keypad.h"
#include "Library.h"
#include "Settings.h"
#include "View.h"

// Liga os gestos do Keypad a biblioteca, preferências e áudio, e descreve o estado para a Ui.
class Player {
public:
    Player(Library& library, Settings& settings, Audio& audio)
        : library_(library)
        , settings_(settings)
        , audio_(audio) {}

    void begin(bool mediaOk);
    void reload();
    void update();
    void handle(Keypad::Action action);

    void pause() { togglePlayIf(false); }
    void play() { togglePlayIf(true); } // vale só com fone conectado. Player::update toca a faixa atual
    bool wantsPlay() const { return playing(); }
    const Library& library() const { return library_; }
    uint8_t playlistIndex() const { return playlist_; }
    bool seeking() const { return seek_.active; }
    void choosePlaylist(uint8_t index);
    View view() const;

private:
    static constexpr uint8_t kVolumeStep = 2;
    static constexpr uint32_t kSaveIntervalMs = 15000; // posição vai para a NVS a cada tanto: o desligar e pela chave

    // Onde a faixa atual retoma quando o Audio não a tem carregada.
    struct Resume {
        uint32_t startMs = 0;        // onde a faixa começa quando for carregada (posição guardada ou busca sem fone)
        uint32_t idleDurationMs = 0; // duração da faixa ainda não carregada, lida ao buscar nela (0 = não lida)
        uint32_t lastPosMs = 0;      // última posição vista com a faixa carregada: o fone pode cair e levar o Audio
        void clearIdle() { startMs = idleDurationMs = 0; }
    };

    // Modo busca. active e dirty não são exclusivos: dirty sobrevive até o destino ser aplicado.
    struct Seek {
        bool active = false;
        bool dirty = false;
        uint32_t targetMs = 0;  // destino ainda não aplicado
        uint32_t lastKeyAt = 0; // última tecla do modo
    };

    bool playing() const { return wantPlay_ || audio_.state() == Audio::State::Playing; }
    void togglePlay();
    void togglePlayIf(bool wanted);
    void step(int direction);
    void selectTrack(uint16_t index);
    void dropTrackIfPhoneGone();
    void saveProgress(uint32_t now);
    void armPlayback();
    void nextPlaylist();
    void changeVolume(int delta);
    void startTrack();
    void remember();
    void handleSeek(Keypad::Action action);
    void seekBy(int direction, bool hold);
    void commitSeek();
    void updateSeek(uint32_t now);
    uint32_t durationMs();
    uint32_t positionMs() const { return loaded_ ? audio_.positionMs() : resume_.startMs; }
    void showTrack();

    Library& library_;
    Settings& settings_;
    Audio& audio_;

    bool mediaOk_ = false;
    bool wantPlay_ = false; // quer tocar assim que houver fone
    bool loaded_ = false;   // o Audio tem a faixa atual
    uint8_t playlist_ = 0;
    uint8_t playlistFrom_ = 0;
    uint16_t track_ = 0;
    char playlistName_[mrm::kMaxPlaylistLen + 1] = ""; // cópias: a Library só guarda uma janela de nomes
    char trackName_[mrm::kMaxTrackLen + 1] = "";

    Resume resume_;
    uint32_t savedAt_ = 0;
    Seek seek_;

    uint32_t trackAt_ = 0;
    uint32_t volumeAt_ = 0;      // último passo de volume
    uint32_t volumeShownAt_ = 0; // quando o painel de volume apareceu
    uint8_t volumeBefore_ = 0;
    uint32_t playlistAt_ = 0;
};
