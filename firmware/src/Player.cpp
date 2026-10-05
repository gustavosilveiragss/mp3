#include "Player.h"

#include "Seek.h"

void Player::begin(bool mediaOk) {
    mediaOk_ = mediaOk;
    audio_.setVolume(settings_.volume());
    reload();
}

// Lê o cartão de novo (depois do portal mexer nele) e volta para a playlist e faixa guardadas.
void Player::reload() {
    if (!mediaOk_)
        return;

    library_.scan();
    playlist_ = 0;
    const bool restored = library_.playlistIndex(settings_.playlist(), playlist_);
    strlcpy(playlistName_, library_.playlistName(playlist_), sizeof(playlistName_));

    track_ = 0;
    resume_.clearIdle();
    loaded_ = false;
    wantPlay_ = false;
    seek_.active = seek_.dirty = false;

    const bool hasTracks = library_.select(playlist_) && library_.trackCount() > 0;
    // A faixa volta pelo nome (uma passada no cartão). Renomeada ou apagada, volta pelo índice guardado.
    if (restored && hasTracks) {
        if (library_.trackIndex(settings_.trackName(), track_))
            resume_.startMs = settings_.positionSec() * 1000; // a posição só vale para a mesma faixa
        else
            track_ = min<uint16_t>(settings_.track(), library_.trackCount() - 1);
    }

    showTrack();

    if (resume_.startMs > 0)
        durationMs(); // com a posição guardada a barra já sai preenchida, antes do play
}

void Player::showTrack() {
    strlcpy(trackName_, library_.trackName(track_), sizeof(trackName_));
}

void Player::update() {
    audio_.update();
    dropTrackIfPhoneGone();

    if (audio_.state() == Audio::State::Finished)
        step(+1);

    const uint32_t now = millis();
    updateSeek(now);
    saveProgress(now);
    armPlayback();
}

// Sem dispositivo a faixa se perde e o play não fica "armado". A posição e a duração ficam para quando o fone voltar.
void Player::dropTrackIfPhoneGone() {
    if (audio_.connected())
        return;

    if (loaded_) {
        resume_.startMs = resume_.lastPosMs;
        resume_.idleDurationMs = audio_.durationMs();
    }

    loaded_ = false;
    wantPlay_ = false;
}

void Player::saveProgress(uint32_t now) {
    if (!loaded_ || !audio_.connected())
        return;

    resume_.lastPosMs = audio_.positionMs();

    const bool dueToSave = now - savedAt_ >= kSaveIntervalMs;
    if (audio_.state() == Audio::State::Playing && dueToSave) {
        savedAt_ = now;
        remember();
    }
}

void Player::armPlayback() {
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

    if (seek_.active) {
        handleSeek(action);
        return;
    }

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

    case Action::Seek:
        seek_.active = true;
        seek_.lastKeyAt = millis();
        break;

    default:
        break;
    }
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
    v.playlist = playlistName_;
    v.playlistIndex = playlist_;
    v.playlistCount = library_.playlistCount();

    v.track = trackName_;
    v.artist = loaded_ ? audio_.artist() : "";
    v.trackIndex = track_;
    v.trackCount = library_.trackCount();

    const uint32_t ms = seek_.dirty ? seek_.targetMs : positionMs();
    const uint32_t duration = loaded_ ? audio_.durationMs() : resume_.idleDurationMs;
    v.seekMode = seek_.active;
    v.positionSec = min<uint32_t>(ms / 1000, UINT16_MAX);

    if (duration > 0)
        v.progress = min<uint32_t>(100, ms * 100ull / duration);

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

    if (!wanted) {
        audio_.pause();
        remember();
    }
}

void Player::step(int direction) {
    const uint16_t count = library_.trackCount();
    if (count == 0)
        return;

    selectTrack((track_ + count + direction) % count);
    wantPlay_ = true;
}

void Player::nextPlaylist() {
    choosePlaylist((playlist_ + 1) % library_.playlistCount());
}

// Troca de playlist: começa na primeira faixa e segue tocando ou pausado, como estava.
void Player::choosePlaylist(uint8_t index) {
    if (index >= library_.playlistCount() || index == playlist_)
        return;

    playlistFrom_ = playlist_;
    playlist_ = index;
    strlcpy(playlistName_, library_.playlistName(playlist_), sizeof(playlistName_));

    library_.select(playlist_);
    selectTrack(0);
    playlistAt_ = trackAt_;
}

void Player::selectTrack(uint16_t index) {
    track_ = index;
    showTrack();
    trackAt_ = stamp();

    loaded_ = false;
    resume_.clearIdle();
    seek_.dirty = false;
    remember();
}

void Player::changeVolume(int delta) {
    const uint32_t now = stamp();
    volumeBefore_ = settings_.volume();
    settings_.setVolume(constrain(int(volumeBefore_) + delta, 0, int(Settings::kMaxVolume)));
    audio_.setVolume(settings_.volume());

    if (now - volumeAt_ >= View::kVolumeVisibleMs) // painel já saiu: a animação de entrada toca de novo
        volumeShownAt_ = now;

    volumeAt_ = now;
}

void Player::startTrack() {
    char path[dirlist::kPath];
    loaded_ = library_.trackPath(trackName_, path, sizeof(path)) && audio_.play(path, resume_.startMs);

    if (!loaded_)
        wantPlay_ = false; // faixa ilegível: para em vez de pular em loop
}

void Player::remember() {
    settings_.setPosition(playlistName_, track_, trackName_, positionMs() / 1000);
}
