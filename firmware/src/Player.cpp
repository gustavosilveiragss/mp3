#include "Player.h"

#include "Seek.h"

namespace {
constexpr uint32_t kSaveMs = 15000; // posicao dentro da faixa vai para a NVS a cada tanto: o desligar e pela chave
}

void Player::begin(bool mediaOk) {
    mediaOk_ = mediaOk;
    audio_.setVolume(settings_.volume());
    reload();
}

// Le o cartao de novo (depois do portal mexer nele) e volta para a playlist e faixa guardadas.
void Player::reload() {
    if (!mediaOk_)
        return;
    library_.scan();
    playlist_ = 0;
    const bool restored = library_.playlistIndex(settings_.playlist(), playlist_);
    strlcpy(playlistName_, library_.playlistName(playlist_), sizeof(playlistName_));
    track_ = 0;
    startMs_ = idleDurationMs_ = 0;
    loaded_ = false;
    wantPlay_ = false;
    seeking_ = seekDirty_ = false;
    const bool hasTracks = library_.select(playlist_) && library_.trackCount() > 0;
    // A faixa volta pelo nome (uma passada no cartao); renomeada ou apagada, pelo indice guardado.
    if (restored && hasTracks) {
        if (library_.trackIndex(settings_.trackName(), track_))
            startMs_ = settings_.positionSec() * 1000; // a posicao so vale para a mesma faixa
        else
            track_ = min<uint16_t>(settings_.track(), library_.trackCount() - 1);
    }
    showTrack();
    if (startMs_ > 0)
        durationMs(); // com a posicao guardada a barra ja sai preenchida, antes do play
}

void Player::showTrack() {
    strlcpy(trackName_, library_.trackName(track_), sizeof(trackName_));
}

void Player::update() {
    audio_.update();
    if (!audio_.connected()) { // sem dispositivo a faixa se perde, e o play nao fica "armado"
        if (loaded_) {         // mas a posicao e a duracao ficam para quando o fone voltar
            startMs_ = lastPosMs_;
            idleDurationMs_ = audio_.durationMs();
        }
        loaded_ = false;
        wantPlay_ = false;
    }
    if (audio_.state() == Audio::State::Finished)
        step(+1);
    const uint32_t now = millis();
    updateSeek(now);
    if (loaded_ && audio_.connected()) {
        lastPosMs_ = audio_.positionMs();
        if (audio_.state() == Audio::State::Playing && now - savedAt_ >= kSaveMs) {
            savedAt_ = now;
            remember();
        }
    }
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
    if (seeking_) {
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
        seeking_ = true;
        seekAt_ = millis();
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
    const uint32_t ms = seekDirty_ ? seekMs_ : positionMs();
    const uint32_t duration = loaded_ ? audio_.durationMs() : idleDurationMs_;
    v.seekMode = seeking_;
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
    track_ = (track_ + count + direction) % count;
    trackAt_ = stamp();
    loaded_ = false;
    startMs_ = idleDurationMs_ = 0;
    seekDirty_ = false;
    wantPlay_ = true;
    showTrack();
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
    strlcpy(playlistName_, library_.playlistName(playlist_), sizeof(playlistName_));
    library_.select(playlist_);
    track_ = 0;
    showTrack();
    trackAt_ = playlistAt_ = stamp();
    loaded_ = false;
    startMs_ = idleDurationMs_ = 0;
    seekDirty_ = false;
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
    char path[dirlist::kPath];
    loaded_ = library_.trackPath(trackName_, path, sizeof(path)) && audio_.play(path, startMs_);
    if (!loaded_)
        wantPlay_ = false; // faixa ilegivel: para em vez de pular em loop
}

void Player::remember() {
    settings_.setPosition(playlistName_, track_, trackName_, positionMs() / 1000);
}
