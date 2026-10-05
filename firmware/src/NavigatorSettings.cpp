#include "Navigator.h"

#include "Device.h"

void Navigator::selectPlaylist() {
    if (cursor_.pos == menu::kPlaylistsBack) {
        back();
        return;
    }

    player_.choosePlaylist(cursor_.pos - menu::kPlaylistsFirst);
    enter(Screen::Home); // a troca de playlist anima na Home
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
        enter(Screen::Playlists,
              min<uint8_t>(player_.playlistIndex() + menu::kPlaylistsFirst, player_.library().playlistCount()));
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

void Navigator::startWifi() {
    resumeAfterWifi_ = player_.wantsPlay();

    player_.pause();          // pausa e fecha o stream antes de o portal poder apagar ou renomear
    audio_.setEnabled(false); // Wi-Fi e Bluetooth não convivem

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
        player_.reload(); // o portal mexeu no cartão

    refreshStorage();
    cursor_.reset();

    const bool redial = settings_.bluetooth() && settings_.device()[0];
    resumeAfterWifi_ = resumeAfterWifi_ && redial;

    audio_.setEnabled(settings_.bluetooth()); // o Wi-Fi já caiu (heap livre), a conexão corre em segundo plano

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

void Navigator::selectGames() {
    if (cursor_.pos == menu::kGamesBack) {
        back();
        return;
    }

    games_.open(cursor_.pos);
    enter(Screen::Play);
}

void Navigator::selectWifi() {
    if (wifiOn_)
        portal_.requestStop(); // a parada roda no update, nunca dentro do tick do Portal
    else if (cursor_.pos == menu::kWifiToggle)
        startWifi();
    else
        back();
}

void Navigator::selectLanguage() {
    if (cursor_.pos == menu::kLanguageBack)
        back();
    else
        setLanguage(static_cast<mrm::Lang>(cursor_.pos));
}

void Navigator::setLanguage(mrm::Lang lang) {
    settings_.setLang(lang);
    locale_.set(lang);
    enter(Screen::Config, menu::kConfigLanguage);
}
