#include "Player.h"

namespace {
uint32_t stamp() {
    return max<uint32_t>(millis(), 1);
} // 0 significa "nunca" na View
} // namespace

void Player::begin(bool mediaOk) {
    mediaOk_ = mediaOk;
    activityAt_ = stamp();
    audio_.setVolume(settings_.volume());
    reload();
}

// Le o cartao de novo (depois do portal mexer nele) e volta para a playlist e faixa guardadas.
void Player::reload() {
    if (!mediaOk_)
        return;
    library_.scan();
    bool found = false;
    playlist_ = 0;
    for (uint8_t p = 0; p < library_.playlistCount() && !found; ++p) {
        found = strcmp(library_.playlistName(p), settings_.playlist()) == 0;
        if (found)
            playlist_ = p;
    }
    track_ = 0;
    loaded_ = false;
    wantPlay_ = false;
    if (library_.select(playlist_) && library_.trackCount() > 0)
        track_ = min<uint16_t>(settings_.track(), library_.trackCount() - 1);
}

void Player::touch() {
    activityAt_ = stamp();
}

void Player::update() {
    audio_.update();
    if (!audio_.connected()) { // sem dispositivo a faixa se perde, e o play nao fica "armado"
        loaded_ = false;
        wantPlay_ = false;
    }
    if (audio_.state() == Audio::State::Finished)
        step(+1);
    if (!wantPlay_ || !audio_.connected())
        return;
    if (loaded_ && audio_.state() == Audio::State::Paused)
        audio_.resume();
    else if (!loaded_)
        startTrack();
}

void Player::handle(Keypad::Action action) {
    using Action = Keypad::Action;
    if (action == Action::None)
        return;
    activityAt_ = stamp();
    if (action == Action::NextPlaylist && library_.playlistCount() > 1) {
        nextPlaylist(); // vale mesmo com a playlist atual vazia
        return;
    }
    if (library_.trackCount() == 0)
        return;

    switch (action) {
    case Action::PlayPause:
        togglePlay();
        break;
    case Action::Next:
        step(+1);
        break;
    case Action::Prev:
        step(-1);
        break;
    case Action::VolUp:
        changeVolume(+kVolumeStep);
        break;
    case Action::VolDown:
        changeVolume(-kVolumeStep);
        break;
    case Action::NextPlaylist:
        nextPlaylist();
        break;
    default:
        break;
    }
}

bool Player::idle() const {
    return !playing() && millis() - activityAt_ >= kIdleMs;
}

View Player::view() const {
    View v;
    v.trackAt = trackAt_;
    v.volumeAt = volumeAt_;
    v.volumeShownAt = volumeShownAt_;
    v.volumeBefore = volumeBefore_;
    v.playlistAt = playlistAt_;
    v.playlistPrev = playlistFrom_;
    v.volume = settings_.volume();
    v.playlist = library_.playlistName(playlist_);
    v.playlistIndex = playlist_;
    v.playlistCount = library_.playlistCount();
    v.track = library_.trackName(track_);
    v.trackIndex = track_;
    v.trackCount = library_.trackCount();
    if (loaded_ && audio_.durationMs() > 0) {
        v.positionSec = audio_.positionMs() / 1000;
        v.progress = min<uint32_t>(100, audio_.positionMs() * 100ull / audio_.durationMs());
    }

    if (!mediaOk_)
        v.mode = View::Mode::NoMedia;
    else if (v.trackCount == 0)
        v.mode = View::Mode::Empty;
    else
        v.mode = playing() && audio_.connected() ? View::Mode::Playing : View::Mode::Paused;
    return v;
}

void Player::togglePlay() {
    togglePlayIf(!playing());
}

void Player::togglePlayIf(bool wanted) {
    wantPlay_ = wanted;
    if (!wanted)
        audio_.pause();
}

void Player::step(int direction) {
    const uint16_t count = library_.trackCount();
    if (count == 0)
        return;
    track_ = (track_ + count + direction) % count;
    trackAt_ = stamp();
    loaded_ = false;
    wantPlay_ = true;
    remember();
}

void Player::nextPlaylist() {
    choosePlaylist((playlist_ + 1) % library_.playlistCount());
}

// Troca de playlist: comeca na primeira faixa e segue tocando ou pausado, como estava.
void Player::choosePlaylist(uint8_t index) {
    if (index >= library_.playlistCount() || index == playlist_)
        return;
    playlistFrom_ = playlist_;
    playlist_ = index;
    library_.select(playlist_);
    track_ = 0;
    trackAt_ = playlistAt_ = stamp();
    loaded_ = false;
    remember();
}

void Player::changeVolume(int delta) {
    const uint32_t now = stamp();
    volumeBefore_ = settings_.volume();
    settings_.setVolume(constrain(int(volumeBefore_) + delta, 0, int(Settings::kMaxVolume)));
    audio_.setVolume(settings_.volume());
    if (now - volumeAt_ >= View::kVolumeVisibleMs) // painel ja saiu: a animacao de entrada toca de novo
        volumeShownAt_ = now;
    volumeAt_ = now;
}

void Player::startTrack() {
    char path[96];
    loaded_ = library_.trackPath(track_, path, sizeof(path)) && audio_.play(path);
    if (!loaded_)
        wantPlay_ = false; // faixa ilegivel: para em vez de pular em loop
}

void Player::remember() {
    settings_.setPosition(library_.playlistName(playlist_), track_);
}
