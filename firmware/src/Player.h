#pragma once

#include "Audio.h"
#include "Keypad.h"
#include "Library.h"
#include "Settings.h"
#include "View.h"

// Liga os gestos do Keypad a biblioteca, preferencias e audio, e descreve o estado para a Ui.
class Player {
public:
    static constexpr uint32_t kIdleMs = 30000; // pausado e sem toque: o display pode apagar

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
    bool idle() const;
    void touch();
    const Library& library() const { return library_; }
    uint8_t playlistIndex() const { return playlist_; }
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

    Library& library_;
    Settings& settings_;
    Audio& audio_;

    bool mediaOk_ = false;
    bool wantPlay_ = false; // quer tocar assim que houver fone
    bool loaded_ = false;   // o Audio tem a faixa atual
    uint8_t playlist_ = 0;
    uint8_t playlistFrom_ = 0;
    uint16_t track_ = 0;

    uint32_t activityAt_ = 0;
    uint32_t trackAt_ = 0;
    uint32_t volumeAt_ = 0;      // ultimo passo de volume
    uint32_t volumeShownAt_ = 0; // quando o painel de volume apareceu
    uint8_t volumeBefore_ = 0;
    uint32_t playlistAt_ = 0;
};
