#pragma once

#include <Arduino.h>
#include <PathRules.h>

#include "DirWindow.h"

constexpr char kMusicDir[] = "/music";

// Playlists sao as pastas de /music, faixas sao os .mp3 dentro delas, tudo em ordem de nome.
// Nada fica em RAM alem de uma janela de nomes por lista: o cartao pode ter qualquer quantidade de musicas.
// Os limites existem por causa do cursor de 8 bits do menu e do tempo de uma varredura, nao da memoria.
class Library {
public:
    static constexpr uint8_t kMaxPlaylists = 250;
    static constexpr uint16_t kMaxTracks = 1000;

    Library()
        : playlists_(true)
        , tracks_(false) {}

    void scan(); // volta a contar as playlists e esquece a selecionada
    uint8_t playlistCount() const { return static_cast<uint8_t>(min<uint16_t>(playlists_.total(), kMaxPlaylists)); }
    // Ponteiros valem ate a proxima consulta: quem precisa guardar o nome copia.
    const char* playlistName(uint8_t playlist) const { return playlists_.name(playlist); }
    bool playlistIndex(const char* name, uint8_t& index) const;
    uint16_t trackCount(uint8_t playlist) const; // em cache por linha, lida do cartao so quando a linha aparece
    uint32_t totalTracks() const;                // varre tudo: so para a tela do cartao

    bool select(uint8_t playlist);
    uint16_t trackCount() const { return min<uint16_t>(tracks_.total(), kMaxTracks); }
    bool trackIndex(const char* name, uint16_t& index) const { return tracks_.indexOf(name, index) && index < kMaxTracks; }
    const char* trackName(uint16_t track) const { return track < trackCount() ? tracks_.name(track) : ""; }
    bool trackPath(const char* track, char* out, size_t size) const;

private:
    mutable DirWindow playlists_;
    mutable DirWindow tracks_;
    char selected_[mrm::kMaxPlaylistLen + 1] = "";
};
