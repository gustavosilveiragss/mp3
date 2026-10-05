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

    if (action == Action::PlayPause || action == Action::VolDown) // A, também segurando
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
    chordTaps_ = 0; // toques pendentes de A+B não valem na tela nova

    if (screen == Screen::Storage)
        refreshStorage(); // custa ler o FS: só ao entrar, nunca por quadro
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
        portal_.handle(); // bloqueia durante um arquivo, o tick do Portal mantém teclas e OLED vivos
        if (portal_.doneRequested())
            stopWifi();
    }

    resumePlayback();

    switch (screen_) {
    case Screen::Splash:
        updateSplash();
        break;

    case Screen::Connecting:
        updateConnecting();
        break;

    case Screen::Home:
        retryConnection();
        break;
    case Screen::Play:
        updatePlay();
        break;

    default:
        break;
    }
}

void Navigator::updateSplash() {
    if (millis() - screenAt_ < kSplashMs)
        return;

    if (settings_.bluetooth())
        reconnect();
    else
        enter(Screen::Home);
}

void Navigator::updateConnecting() {
    if (audio_.connected()) {
        settings_.setDevice(audio_.peer(), audio_.peerBda());
        enter(Screen::Home);
    } else if (audio_.link() != Audio::Link::Connecting || millis() - screenAt_ >= kConnectTimeoutMs) {
        connectFailed();
    }
}

void Navigator::updatePlay() {
    games_.update(millis(), aHeld_, bHeld_);

    if (aHeld_ || bHeld_ || games_.running())
        wake(); // jogar não e ficar ocioso, e o fim ou a pausa ainda ficam 10 s na tela

    if (games_.exited())
        enter(Screen::Games, games_.index());
}

void Navigator::handleHome(Keypad::Action action) {
    using Action = Keypad::Action;

    const bool seeking = player_.seeking(); // A, B e A+B têm outro papel e valem sem fone
    const bool isTransport = action == Action::PlayPause || action == Action::Next || action == Action::Prev;
    const bool needsDevice = !seeking && isTransport;

    if (action == Action::NextPlaylist && settings_.lockEnabled() && !seeking)
        countChord(); // com o bloqueio ligado A+B espera a contagem: 3 toques travam, 1 ou 2 trocam de playlist
    else if (action == Action::OpenMenu)
        enter(Screen::Config);

    else if (!settings_.bluetooth())
        return; // sem Bluetooth a Home é só uma mensagem: só a Config faz sentido
    else if (needsDevice && !audio_.connected())
        showToast(View::Toast::NoDevice); // a Home já explica, só a Config (A+B segurado) leva a escolher um

    else
        player_.handle(action);
}

// Travado: só os 3 toques de A+B fazem algo. Qualquer outro botão só lembra como destravar.
void Navigator::handleLocked(Keypad::Action action) {
    if (action == Keypad::Action::NextPlaylist)
        countChord();
    else
        showToast(View::Toast::LockedHint);
}

void Navigator::countChord() {
    chordAt_ = stamp();

    if (++chordTaps_ < kLockTaps)
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
        toastShownAt_ = now; // a animação de entrada só toca quando o aviso estava fora da tela

    toast_ = toast;
    toastAt_ = now;
}

// Quem deve acender a tela apagada sem uma tecla ter sido engolida: aviso de bloqueio ou troca de playlist.
bool Navigator::wantsDisplay() const {
    const bool toastWakes = toast_ == View::Toast::Locked || toast_ == View::Toast::Unlocked;
    return (toastWakes && toastActive()) || (deliveredAt_ && millis() - deliveredAt_ < kDeliveredWakeMs);
}

// O aviso "A+B 3x desbloqueia" não acende a tela: no bolso qualquer toque ia acender.
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

    const bool down = action == Action::PlayPause || action == Action::VolDown;       // A, também segurando
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

void Navigator::select() {
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
        selectGames();
        break;
    case Screen::Wifi:
        selectWifi();
        break;

    case Screen::Bluetooth:
        selectBluetooth();
        break;
    case Screen::Language:
        selectLanguage();
        break;

    default:
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

void Navigator::wake() {
    activityAt_ = stamp();
}

bool Navigator::idle() const {
    if (wifiOn_ || screen_ == Screen::Splash || gameRunning())
        return false;

    // desligada, a tela nunca apaga. Ligada, apaga parada mesmo tocando
    return settings_.screenOff() && millis() - activityAt_ >= kScreenOffMs;
}
