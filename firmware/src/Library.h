#pragma once

#include <Arduino.h>
#include <FS.h>

#include <PathRules.h>

constexpr char kMusicDir[] = "/music";

// Playlists sao as pastas de /music, faixas sao os .mp3 dentro delas, tudo em ordem de nome.
// scan() lista as playlists; select() carrega as faixas de uma, que e a unica que o Player precisa.
class Library {
public:
    static constexpr uint8_t kMaxPlaylists = 16;
    static constexpr uint16_t kMaxTracks = 128; // alem disso as faixas sobram sem tocar

    explicit Library(fs::FS& fs)
        : fs_(fs) {}

    void scan();
    uint8_t playlistCount() const { return playlists_; }
    const char* playlistName(uint8_t playlist) const { return playlist < playlists_ ? names_[playlist] : ""; }
    uint16_t trackCount(uint8_t playlist) const { return playlist < playlists_ ? counts_[playlist] : 0; }
    bool truncated() const { return truncated_; } // passou de kMaxPlaylists ou kMaxTracks

    bool select(uint8_t playlist);
    uint16_t trackCount() const { return trackCount_; }
    const char* trackName(uint16_t track) const { return track < trackCount_ ? tracks_[track] : ""; }
    bool trackPath(uint16_t track, char* out, size_t size) const;

private:
    static constexpr uint8_t kNone = 0xff;

    uint16_t countTracks(const char* playlist);

    fs::FS& fs_;
    char names_[kMaxPlaylists][mrm::kMaxPlaylistLen + 1];
    uint16_t counts_[kMaxPlaylists];
    uint8_t playlists_ = 0;

    char tracks_[kMaxTracks][mrm::kMaxTrackLen + 1];
    uint16_t trackCount_ = 0;
    uint8_t selected_ = kNone;
    bool truncated_ = false;
};
