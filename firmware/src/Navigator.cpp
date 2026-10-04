#include "Navigator.h"

#include "Device.h"
#include "Media.h"

namespace {
uint8_t percentUsed(uint64_t used, uint64_t total) {
    return total ? static_cast<uint8_t>(min<uint64_t>(100, used * 100 / total)) : 0;
}

mrm::game::Nav gameNav(Keypad::Action action) {
    using Action = Keypad::Action;
    using mrm::game::Nav;
    if (action == Action::PlayPause || action == Action::VolDown) // A, tambem segurando
        return Nav::Down;
    if (action == Action::Next) // B
        return Nav::Confirm;
    if (action == Action::NextPlaylist || action == Action::OpenMenu)
        return Nav::Back;
    return Nav::None;
}
} // namespace

void Navigator::begin() {
    mrm::deviceCode(code_);
    activityAt_ = stamp();
    enter(Screen::Splash);
}

void Navigator::enter(Screen screen, uint8_t cursor) {
    screen_ = screen;
    screenAt_ = stamp();
    wake(); // trocar de tela e atividade: sem isso a Home nasce ociosa depois de um tempo nos menus
    cursor_.reset(cursor);
    chordTaps_ = 0; // toques pendentes de A+B nao valem na tela nova
    if (screen == Screen::Storage)
        refreshStorage(); // custa ler o FS: so ao entrar, nunca por quadro
}

void Navigator::refreshStorage() {
    const Library& lib = player_.library();
    storage_ = {};
    storage_.sd = media::sdPresent();
    storage_.totalBytes = media::totalBytes();
    storage_.freeBytes = media::freeBytes();
    storage_.usedPercent = percentUsed(media::usedBytes(), storage_.totalBytes);
    storage_.playlists = lib.playlistCount();
    storage_.tracks = static_cast<uint16_t>(min<uint32_t>(lib.totalTracks(), UINT16_MAX));
}

void Navigator::update() {
    player_.update();
    resolveChord();
    if (wifiOn_) {
        portal_.handle(); // bloqueia durante um arquivo; o tick do Portal mantem teclas e OLED vivos
        if (portal_.doneRequested())
            stopWifi();
    }
    resumePlayback();
    switch (screen_) {
    case Screen::Splash:
        if (millis() - screenAt_ < kSplashMs)
            break;
        if (!settings_.bluetooth())
            enter(Screen::Home);
        else
            reconnect();
        break;
    case Screen::Connecting:
        if (audio_.connected()) {
            settings_.setDevice(audio_.peer(), audio_.peerBda());
            enter(Screen::Home);
        } else if (audio_.link() != Audio::Link::Connecting || millis() - screenAt_ >= kConnectTimeoutMs) {
            connectFailed();
        }
        break;
    case Screen::Home:
        retryConnection();
        break;
    case Screen::Play:
        games_.update(millis(), aHeld_, bHeld_);
        if (aHeld_ || bHeld_ || games_.running())
            wake(); // jogar nao e ficar ocioso, e o fim ou a pausa ainda ficam 10 s na tela
        if (games_.exited())
            enter(Screen::Games, games_.index());
        break;
    default:
        break;
    }
}

void Navigator::handleHome(Keypad::Action action) {
    using Action = Keypad::Action;
    const bool needsDevice = action == Action::PlayPause || action == Action::Next || action == Action::Prev;
    if (action == Action::NextPlaylist && settings_.lockEnabled())
        countChord(); // com o bloqueio ligado A+B espera a contagem: 3 toques travam, 1 ou 2 trocam de playlist
    else if (action == Action::OpenMenu)
        enter(Screen::Config);
    else if (!settings_.bluetooth())
        return; // sem Bluetooth a Home e so uma mensagem: so a Config faz sentido
    else if (needsDevice && !audio_.connected())
        showToast(View::Toast::NoDevice); // a Home ja explica; so a Config (A+B segurado) leva a escolher um
    else
        player_.handle(action);
}

// Travado: so os 3 toques de A+B fazem algo. Qualquer outro botao so lembra como destravar.
void Navigator::handleLocked(Keypad::Action action) {
    if (action == Keypad::Action::NextPlaylist)
        countChord();
    else
        showToast(View::Toast::LockedHint);
}

void Navigator::countChord() {
    chordAt_ = stamp();
    if (++chordTaps_ < 3)
        return;
    chordTaps_ = 0;
    locked_ = !locked_;
    showToast(locked_ ? View::Toast::Locked : View::Toast::Unlocked);
}

// Passada a janela sem o terceiro toque, entrega os toques como troca de playlist (ou descarta, se travado).
void Navigator::resolveChord() {
    if (!chordTaps_ || millis() - chordAt_ < kChordGapMs)
        return;
    const uint8_t taps = chordTaps_;
    chordTaps_ = 0;
    if (locked_ || screen_ != Screen::Home)
        return;
    deliveredAt_ = stamp(); // o toque acende a tela, que estava apagada contando
    if (!settings_.bluetooth())
        return;
    for (uint8_t i = 0; i < taps; ++i)
        player_.handle(Keypad::Action::NextPlaylist);
}

void Navigator::showToast(View::Toast toast) {
    const uint32_t now = stamp();
    if (now - toastAt_ >= View::kToastHoldMs + View::kToastEdgeMs)
        toastShownAt_ = now; // a animacao de entrada so toca quando o aviso estava fora da tela
    toast_ = toast;
    toastAt_ = now;
}

// Quem deve acender a tela apagada sem uma tecla ter sido engolida: aviso de bloqueio ou troca de playlist.
bool Navigator::wantsDisplay() const {
    const bool toastWakes = toast_ == View::Toast::Locked || toast_ == View::Toast::Unlocked;
    return (toastWakes && toastActive()) || (deliveredAt_ && millis() - deliveredAt_ < 200);
}

// O aviso "A+B 3x desbloqueia" nao acende a tela: no bolso qualquer toque ia acender.
bool Navigator::toastActive() const {
    return toast_ != View::Toast::None && millis() - toastAt_ < View::kToastHoldMs + View::kToastEdgeMs;
}

void Navigator::handle(Keypad::Action action) {
    using Action = Keypad::Action;
    if (action == Action::None)
        return;
    wake();
    if (locked_) {
        handleLocked(action);
        return;
    }
    if (screen_ == Screen::Home) {
        handleHome(action);
        return;
    }
    if (screen_ == Screen::Splash)
        return;
    if (screen_ == Screen::Play) {
        games_.handle(gameNav(action));
        return;
    }

    const bool down = action == Action::PlayPause || action == Action::VolDown;       // A, tambem segurando
    const bool confirm = action == Action::Next;                                      // B
    const bool cancel = action == Action::NextPlaylist || action == Action::OpenMenu; // A+B

    if (down) {
        cursor_.move(rows());
    } else if (confirm) {
        select();
    } else if (cancel) {
        back();
    }
}

uint8_t Navigator::rows() const {
    switch (screen_) {
    case Screen::Picker:
        return audio_.deviceCount() + 2; // + buscar de novo + voltar
    case Screen::Config:
        return menu::kConfigRows;
    case Screen::Wifi:
        return wifiOn_ ? 1 : menu::kWifiRows;
    case Screen::Bluetooth:
        return menu::kBluetoothRows;
    case Screen::Language:
        return menu::kLanguageRows;
    case Screen::Playlists:
        return player_.library().playlistCount() + menu::kPlaylistsFirst;
    case Screen::Games:
        return menu::kGamesRows;
    case Screen::Extras:
        return menu::kExtrasRows;
    default:
        return 1;
    }
}

void Navigator::selectPlaylist() {
    if (cursor_.pos == menu::kPlaylistsBack) {
        back();
        return;
    }
    player_.choosePlaylist(cursor_.pos - menu::kPlaylistsFirst);
    enter(Screen::Home); // a troca de playlist anima na Home
}

void Navigator::select() {
    const uint8_t pos = cursor_.pos;
    switch (screen_) {
    case Screen::Connecting:
    case Screen::Storage:
        back();
        break;
    case Screen::Picker:
        selectDevice();
        break;
    case Screen::Config:
        selectConfig();
        break;
    case Screen::Extras:
        selectExtras();
        break;
    case Screen::Playlists:
        selectPlaylist();
        break;
    case Screen::Games:
        if (pos == menu::kGamesBack) {
            back();
        } else {
            games_.open(pos);
            enter(Screen::Play);
        }
        break;
    case Screen::Wifi:
        if (wifiOn_)
            portal_.requestStop(); // a parada roda no update, nunca dentro do tick do Portal
        else if (pos == menu::kWifiToggle)
            startWifi();
        else
            back();
        break;
    case Screen::Bluetooth:
        selectBluetooth();
        break;
    case Screen::Language:
        if (pos == menu::kLanguageBack)
            back();
        else
            setLanguage(static_cast<mrm::Lang>(pos));
        break;
    default:
        break;
    }
}

void Navigator::selectExtras() {
    switch (cursor_.pos) {
    case menu::kExtrasScreenOff:
        settings_.setScreenOff(!settings_.screenOff());
        screenOffAt_ = stamp();
        break;
    case menu::kExtrasLock:
        settings_.setLockEnabled(!settings_.lockEnabled());
        lockAt_ = stamp();
        break;
    default:
        back();
        break;
    }
}

void Navigator::selectConfig() {
    switch (cursor_.pos) {
    case menu::kConfigPlaylists:
        enter(Screen::Playlists, min<uint8_t>(player_.playlistIndex() + menu::kPlaylistsFirst, player_.library().playlistCount()));
        break;
    case menu::kConfigBluetooth:
        enter(Screen::Bluetooth);
        break;
    case menu::kConfigWifi:
        enter(Screen::Wifi);
        break;
    case menu::kConfigLanguage:
        enter(Screen::Language, static_cast<uint8_t>(locale_.lang()));
        break;
    case menu::kConfigExtras:
        enter(Screen::Extras);
        break;
    case menu::kConfigStorage:
        enter(Screen::Storage);
        break;
    case menu::kConfigGames:
        enter(Screen::Games);
        break;
    default:
        enter(Screen::Home);
        break;
    }
}

void Navigator::back() {
    switch (screen_) {
    case Screen::Connecting:
        audio_.disconnect();
        connectFailed();
        break;
    case Screen::Picker:
        enter(Screen::Bluetooth, menu::kBluetoothPick);
        break;
    case Screen::Config:
        enter(Screen::Home);
        break;
    case Screen::Playlists:
        enter(Screen::Config, menu::kConfigPlaylists);
        break;
    case Screen::Games:
        enter(Screen::Config, menu::kConfigGames);
        break;
    case Screen::Extras:
        enter(Screen::Config, menu::kConfigExtras);
        break;
    case Screen::Storage:
        enter(Screen::Config, menu::kConfigStorage);
        break;
    case Screen::Wifi:
        if (wifiOn_)
            portal_.requestStop();
        else
            enter(Screen::Config, menu::kConfigWifi);
        break;
    case Screen::Bluetooth:
        enter(Screen::Config, menu::kConfigBluetooth);
        break;
    case Screen::Language:
        enter(Screen::Config, menu::kConfigLanguage);
        break;
    default:
        break;
    }
}

void Navigator::startWifi() {
    resumeAfterWifi_ = player_.wantsPlay();
    player_.pause();          // pausa e fecha o stream antes de o portal poder apagar ou renomear
    audio_.setEnabled(false); // Wi-Fi e Bluetooth nao convivem
    char ssid[sizeof(device::kName) + 6];
    snprintf(ssid, sizeof(ssid), "%s %s", device::kName, code_);
    portal_.begin(ssid);
    wifiOn_ = true;
    wifiAt_ = stamp();
    cursor_.reset();
}

void Navigator::stopWifi() {
    portal_.end();
    wifiOn_ = false;
    wifiAt_ = stamp();
    if (portal_.changed())
        player_.reload(); // o portal mexeu no cartao
    refreshStorage();
    cursor_.reset();
    const bool redial = settings_.bluetooth() && settings_.device()[0];
    resumeAfterWifi_ = resumeAfterWifi_ && redial;
    audio_.setEnabled(settings_.bluetooth()); // o Wi-Fi ja caiu (heap livre); a conexao corre em segundo plano
    if (redial) {
        audio_.connect(settings_.device(), settings_.deviceBda());
        retryAt_ = wifiAt_;
    }
}

void Navigator::resumePlayback() {
    if (!resumeAfterWifi_ || wifiOn_)
        return;
    const bool linked = audio_.connected();
    if (linked)
        player_.play();
    resumeAfterWifi_ = !linked && millis() - wifiAt_ < kResumeMs;
}

void Navigator::setLanguage(mrm::Lang lang) {
    settings_.setLang(lang);
    locale_.set(lang);
    enter(Screen::Config, menu::kConfigLanguage);
}

void Navigator::wake() {
    activityAt_ = stamp();
}

bool Navigator::idle() const {
    if (wifiOn_ || screen_ == Screen::Splash || gameRunning())
        return false;
    // desligada, a tela nunca apaga; ligada, apaga parada mesmo tocando
    return settings_.screenOff() && millis() - activityAt_ >= kScreenOffMs;
}
