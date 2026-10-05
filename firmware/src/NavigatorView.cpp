#include "Navigator.h"

namespace {
constexpr uint32_t kBetweenFilesMs = 6000; // o próximo arquivo da fila ainda deve chegar

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

    const bool live = p.active || (p.more && millis() - p.at < kBetweenFilesMs);
    v.xfer = {live, !live && portal_.changed(), p.file,
              p.files, percent(p.sent, p.size), percent(p.totalSent, p.totalSize)};
}

void Navigator::fillHome(View& v) const {
    v.noDevice = settings_.bluetooth() && !audio_.connected();

    // erro de mídia e biblioteca vazia continuam visíveis
    if (v.mode != View::Mode::Paused && v.mode != View::Mode::Playing)
        return;

    if (!settings_.bluetooth())
        v.mode = View::Mode::BtOff;
    else if (v.noDevice)
        v.mode = View::Mode::NoDevice;
}

View::Mode Navigator::modeOf(Screen screen, View::Mode homeMode) {
    using Mode = View::Mode;

    // na ordem de Screen. Home fica com o modo que o Player escolheu
    static constexpr Mode kModes[] = {Mode::Splash, Mode::Connecting, Mode::Picker, Mode::Playing, Mode::Config,
                                      Mode::Playlists, Mode::Games, Mode::Game, Mode::Wifi, Mode::Bluetooth,
                                      Mode::Language, Mode::Extras, Mode::Storage};

    static_assert(sizeof(kModes) / sizeof(kModes[0]) == static_cast<size_t>(Screen::Storage) + 1,
                  "kModes na ordem de Screen");

    return screen == Screen::Home ? homeMode : kModes[static_cast<uint8_t>(screen)];
}

View Navigator::view() const {
    View v = player_.view();
    fillMenus(v);

    if (screen_ == Screen::Home)
        fillHome(v);
    else if (screen_ == Screen::Play)
        games_.fill(v.game);

    v.mode = modeOf(screen_, v.mode);

    return v;
}
