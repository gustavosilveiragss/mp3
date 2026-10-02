#include "Navigator.h"

namespace {
uint8_t percent(uint32_t part, uint32_t whole) {
    return whole ? static_cast<uint8_t>(min<uint64_t>(100, part * 100ull / whole)) : 0;
}
} // namespace

// Campos dos menus, do aviso e das funcionalidades extras.
void Navigator::fillMenus(View& v) const {
    v.screenAt = screenAt_;
    v.deviceCode = code_;
    v.btLink = audio_.link();
    v.wifiOn = wifiOn_;
    v.peer = audio_.peer();
    v.devices = audio_.devices();
    v.deviceCount = audio_.deviceCount();
    v.scanning = audio_.scanning();
    v.cursor = cursor_.pos;
    v.cursorPrev = cursor_.prev;
    v.cursorAt = cursor_.at;
    v.library = &player_.library();
    v.storage = storage_;
    for (uint8_t i = 0; i < mrm::game::twobtn::kCount; ++i)
        v.best[i] = settings_.best(i);
    v.lang = locale_.lang();
    v.btOn = settings_.bluetooth() && !wifiOn_;
    v.btAt = btAt_;
    v.screenOffOn = settings_.screenOff();
    v.lockOn = settings_.lockEnabled();
    v.screenOffAt = screenOffAt_;
    v.lockAt = lockAt_;
    v.toast = toast_;
    v.toastAt = toastAt_;
    v.toastShownAt = toastShownAt_;
    v.wifiAt = wifiAt_;
    if (!wifiOn_)
        return;
    const Portal::Progress& p = portal_.progress();
    v.wifiClient = portal_.clientConnected();
    v.xfer = {p.active, !p.active && portal_.changed(), p.file, p.files, percent(p.sent, p.size), percent(p.totalSent, p.totalSize)};
}

View Navigator::view() const {
    View v = player_.view();
    fillMenus(v);
    switch (screen_) {
    case Screen::Splash:
        v.mode = View::Mode::Splash;
        break;
    case Screen::Connecting:
        v.mode = View::Mode::Connecting;
        break;
    case Screen::Picker:
        v.mode = View::Mode::Picker;
        break;
    case Screen::Config:
        v.mode = View::Mode::Config;
        break;
    case Screen::Wifi:
        v.mode = View::Mode::Wifi;
        break;
    case Screen::Bluetooth:
        v.mode = View::Mode::Bluetooth;
        break;
    case Screen::Language:
        v.mode = View::Mode::Language;
        break;
    case Screen::Playlists:
        v.mode = View::Mode::Playlists;
        break;
    case Screen::Games:
        v.mode = View::Mode::Games;
        break;
    case Screen::Extras:
        v.mode = View::Mode::Extras;
        break;
    case Screen::Storage:
        v.mode = View::Mode::Storage;
        break;
    case Screen::Play:
        games_.fill(v.game);
        v.mode = View::Mode::Game;
        break;
    case Screen::Home:
        v.noDevice = settings_.bluetooth() && !audio_.connected();
        if (v.mode == View::Mode::Paused || v.mode == View::Mode::Playing) { // erro de midia e biblioteca vazia continuam visiveis
            if (!settings_.bluetooth())
                v.mode = View::Mode::BtOff;
            else if (v.noDevice)
                v.mode = View::Mode::NoDevice;
        }
        break; // Player::view ja escolheu Playing, Paused, Empty ou NoMedia
    }
    return v;
}
