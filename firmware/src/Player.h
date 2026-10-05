#pragma once

#include "Audio.h"
#include "Keypad.h"
#include "Library.h"
#include "Settings.h"
#include "View.h"

// Liga os gestos do Keypad a biblioteca, preferencias e audio, e descreve o estado para a Ui.
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
    void play() { togglePlayIf(true); } // vale so com fone conectado; Player::update toca a faixa atual
    bool wantsPlay() const { return playing(); }
    const Library& library() const { return library_; }
    uint8_t playlistIndex() const { return playlist_; }
    bool seeking() const { return seeking_; }
    void choosePlaylist(uint8_t index);
    View view() const;

private:
    static constexpr uint8_t kVolumeStep = 2;

    bool playing() const { return wantPlay_ || audio_.state() == Audio::State::Playing; }
    void togglePlay();
    void togglePlayIf(bool wanted);
    void step(int direction);
    void nextPlaylist();
    void changeVolume(int delta);
    void startTrack();
    void remember();
    void handleSeek(Keypad::Action action);
    void seekBy(int direction, bool hold);
    void commitSeek();
    void updateSeek(uint32_t now);
    uint32_t durationMs();
    uint32_t positionMs() const { return loaded_ ? audio_.positionMs() : startMs_; }
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
    char playlistName_[mrm::kMaxPlaylistLen + 1] = ""; // copias: a Library so guarda uma janela de nomes
    char trackName_[mrm::kMaxTrackLen + 1] = "";

    uint32_t startMs_ = 0;        // onde a faixa comeca quando for carregada (posicao guardada ou busca sem fone)
    uint32_t idleDurationMs_ = 0; // duracao da faixa ainda nao carregada, lida ao buscar nela (0 = nao lida)
    uint32_t lastPosMs_ = 0;      // ultima posicao vista com a faixa carregada: o fone pode cair e levar o Audio
    uint32_t savedAt_ = 0;

    bool seeking_ = false; // modo busca
    bool seekDirty_ = false;
    uint32_t seekMs_ = 0; // destino ainda nao aplicado
    uint32_t seekAt_ = 0; // ultima tecla do modo

    uint32_t trackAt_ = 0;
    uint32_t volumeAt_ = 0;      // ultimo passo de volume
    uint32_t volumeShownAt_ = 0; // quando o painel de volume apareceu
    uint8_t volumeBefore_ = 0;
    uint32_t playlistAt_ = 0;
};
